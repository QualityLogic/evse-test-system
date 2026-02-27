// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "auth_token_providerImpl.hpp"

#include <everest/staging/helpers/helpers.hpp>

namespace module::token_provider {

void auth_token_providerImpl::init() {
}

void auth_token_providerImpl::ready() {
    mod->signal_new_token.connect([this]() {
        types::authorization::ProvidedIdToken token;

        token.id_token = {config.token, types::authorization::IdTokenType::ISO14443};
        token.authorization_type = types::authorization::string_to_authorization_type(config.type);
        if (config.connector_id > 0) {
            token.connectors.emplace({config.connector_id});
        }
        token.parent_id_token = {config.token, types::authorization::IdTokenType::ISO14443};

        EVLOG_info << "Publishing new dummy token: " << everest::staging::helpers::redact(token);
        publish_provided_token(token);
    });
}

} // namespace module::token_provider