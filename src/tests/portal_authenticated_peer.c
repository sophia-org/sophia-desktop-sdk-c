#define _POSIX_C_SOURCE 200809L
#include "sophia_launch_auth.h"
#include "sophia_namespace.h"
#include "sophia_portal_frame.h"
#include "sophia_portal_startup.h"
#include <assert.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static uint64_t now(void) {
  struct timespec t;
  assert(!clock_gettime(CLOCK_MONOTONIC, &t));
  return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000;
}
int main(void) {
  struct sophia_ns_discovery *names = malloc(sizeof *names);
  struct sophia_9p_client role, relay;
  struct sophia_la_client auth;
  struct sophia_po opening;
  size_t bytes = sophia_9p_storage_bytes(4096, 8);
  void *a = malloc(bytes), *b = malloc(bytes);
  int dir, fd;
  uint64_t deadline = now() + 10000;
  FILE *report;
  int frame_mode = !strcmp(getenv("SOPHIA_TEST_PORTAL_BOUNDARY"), "sdk-frame");
  assert(names && a && b);
  dir = sophia_ns_open(getenv(SOPHIA_NS_DIRECTORY_ENV), names);
  assert(dir >= 0);
  fd = sophia_ns_connect(dir, names, "roles/portal");
  assert(fd >= 0);
  assert(!sophia_9p_init(&role, fd, 4096, 8, 16, a, bytes));
  assert(!sophia_9p_init(&relay, STDIN_FILENO, 4096, 8, 16, b, bytes));
  assert(!sophia_la_init(&auth, &role, &relay, "namespace-client",
                         "portal-client", now()));
  while (!sophia_la_ready(&auth)) {
    struct pollfd p[2] = {
        {fd, POLLIN | (sophia_9p_wants_write(&role) ? POLLOUT : 0), 0},
        {STDIN_FILENO, POLLIN | (sophia_9p_wants_write(&relay) ? POLLOUT : 0),
         0}};
    assert(now() < deadline && poll(p, 2, 10) >= 0);
    assert(!sophia_la_service(&auth, now(), 65536));
  }
  assert(!sophia_po_init(&opening, &role, sophia_la_root(&auth), now(),
                         now() + 2000));
  while (!sophia_po_ready(&opening)) {
    struct pollfd p = {
        fd, POLLIN | (sophia_po_wants_write(&opening) ? POLLOUT : 0), 0};
    uint64_t t = now();
    assert(t < deadline &&
           poll(&p, 1,
                (int)(opening.deadline > t ? opening.deadline - t : 0)) >= 0);
    assert(!sophia_po_dispatch(&opening, now(), 65536));
  }
  assert(opening.info.epoch &&
         opening.info.request == (frame_mode ? SOPHIA_PF_OUTPUT_CAPTURE_V1
                                             : SOPHIA_PF_REQUEST_UNAVAILABLE) &&
         role.fid_count == 1);
  if (frame_mode) {
    struct sophia_pfr frame;
    struct sophia_pf_frame_info info;
    size_t length, i;
    const void *result;
    unsigned char *image = malloc(256 * 128 * 4);
    assert(image && !sophia_pfr_init(&frame, &opening, image, 256 * 128 * 4,
                                     now(), now() + 3000));
    while (!sophia_pfr_result(&frame, &info, &result, &length)) {
      struct pollfd p = {
          fd, POLLIN | (sophia_pfr_wants_write(&frame) ? POLLOUT : 0), 0};
      uint64_t t = now();
      assert(t < deadline &&
             poll(&p, 1, (int)(frame.deadline > t ? frame.deadline - t : 0)) >=
                 0);
      assert(!sophia_pfr_dispatch(&frame, now(), 65536));
    }
    assert(result == image && length == 256 * 128 * 4 && info.output == 7 &&
           info.output_generation == 9 && info.presentation_generation == 11 &&
           info.width == 256 && info.height == 128 && role.fid_count == 1);
    for (i = 0; i < length; ++i)
      assert(image[i] == 17);
    sophia_pfr_close(&frame);
    free(image);
  }
  report = fopen(getenv("SOPHIA_TEST_LOCK_REPORT"), "w");
  assert(report && fputs(frame_mode ? "frame\n" : "started\n", report) >= 0 &&
         !fclose(report));
  sophia_po_close(&opening);
  close(fd);
  close(STDIN_FILENO);
  close(dir);
  free(a);
  free(b);
  free(names);
  return 0;
}
