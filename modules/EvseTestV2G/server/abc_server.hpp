// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#ifndef ABC_SERVER_HPP
#define ABC_SERVER_HPP

#include "v2g.hpp"

/**
 * @class V2GServer
 * @brief Abstract base class for a V2G protocol server.
 */
class V2GServer {
public:
    virtual ~V2GServer() = default;

    /**
     * @brief This function handles the incoming request message of a connected EV.
     *  It analyzes the incoming request message and configures the next response.
     * @param conn Holds the V2G-connection information and provides the EXI streams.
     * @return When this function returns <c>-1</c> then the connection is aborted without sending a reply,
     *  when this function returns <c>V2G_EVENT_NO_EVENT</c> then the reply is sent,
     *  when this function returns <c>V2G_EVENT_TERMINATE_CONNECTION</c> then no reply is sent and the connection is closed,
     *  when this function returns <c>V2G_EVENT_SEND_AND_TERMINATE</c> then the reply is sent and the connection is closed afterwards.
     */
    virtual v2g_event handle_request(v2g_connection* conn) = 0;

    virtual V2gMsgTypeId find_req_message_type(const v2g_connection* conn) = 0;
};

#endif //ABC_SERVER_HPP
