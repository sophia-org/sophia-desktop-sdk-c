#ifndef SOPHIA_SHELL_FILES_DESCRIPTORS_H
#define SOPHIA_SHELL_FILES_DESCRIPTORS_H
#include "sophia_shell_files_roles.h"

/* Development descriptor control values, pinned to spec/proposed/.
 * These passive records do not grant the descriptor role or implement a
 * session. Transactions are body identities, distinct from submission IDs.
 * Decoded query text borrows the immutable source record. */
struct sophia_sf_descriptor_outcome {
    uint64_t transaction, connection_epoch, candidate_generation, presentation_epoch;
    uint16_t kind;
};
struct sophia_sf_descriptor_activation {
    uint64_t transaction, connection_epoch, candidate_generation, presentation_epoch;
    uint64_t activation, action_token, action_issuer_epoch, action_issuer_revocation_epoch;
    uint64_t action_recipient_epoch;
    uint16_t action_target_slot;
    uint64_t action_target_generation;
};
struct sophia_sf_reference_request {
    uint64_t transaction, connection_epoch, catalog_generation, request_generation;
    uint64_t output_id, output_generation, presentation_epoch;
    uint16_t operation;
};
struct sophia_sf_reference_outcome {
    uint64_t transaction, connection_epoch, catalog_generation, request_generation;
    uint64_t candidate_generation, presentation_epoch;
    uint16_t page, pages, kind;
};
struct sophia_sf_descriptor_launcher_request {
    struct sophia_sf_reference_request request;
    struct sophia_sf_text query;
};
struct sophia_sf_descriptor_launcher_outcome {
    uint64_t transaction, connection_epoch, request_generation, candidate_generation;
    uint64_t presentation_epoch;
    uint16_t kind;
};
struct sophia_sf_descriptor_launcher_activation {
    uint64_t transaction, connection_epoch, catalog_generation, request_generation;
    uint64_t candidate_generation, presentation_epoch, activation;
    uint16_t slot;
};
struct sophia_sf_descriptor_launch_outcome {
    struct sophia_sf_descriptor_launcher_activation grant;
    uint16_t status;
};
struct sophia_sf_descriptor_activation_ack {
    uint64_t transaction, connection_epoch, activation;
    uint16_t disposition;
};
struct sophia_sf_descriptor_launcher_activation_ack {
    struct sophia_sf_descriptor_launcher_activation grant;
    uint16_t consumed;
};
#endif
