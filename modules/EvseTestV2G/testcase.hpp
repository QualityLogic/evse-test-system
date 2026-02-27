// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#ifndef TESTCASE_HPP
#define TESTCASE_HPP

#include <optional>
#include <vector>

#include "abc_server.hpp"
#include "din_server.hpp"
#include "iso_server.hpp"
#include "test_events.hpp"
#include "v2g.hpp"

namespace testing {

void report_connection_closed(const v2g_connection* conn, types::test_report::ReportMetadata metadata);

void report_connection_closed(const v2g_connection* conn);

void report_bsp_event(const v2g_connection* conn, types::board_support_common::BspEvent bsp_event,
                      types::test_report::ReportMetadata metadata);

void report_bsp_event(const v2g_connection* conn, types::board_support_common::BspEvent bsp_event);

void report_bsp_measurement(const v2g_connection* conn, types::board_support_common::BspMeasurement bsp_measurement,
                            types::test_report::ReportMetadata metadata);

void report_bsp_measurement(const v2g_connection* conn, types::board_support_common::BspMeasurement bsp_measurement);

struct V2gMsgReportContext {
    std::vector<types::test_report::MessageField> message_fields;
    types::test_report::ReportMetadata metadata;
};

template<typename ResponseCodeT>
struct V2gResReportContext {
    std::optional<ResponseCodeT> response_code;
    std::vector<types::test_report::MessageField> message_fields;
    types::test_report::ReportMetadata metadata;
};

void report_v2g_message(const v2g_connection* conn, types::iso15118::V2gMessageId message_id,
                        V2gMsgReportContext context);

void report_v2g_message(const v2g_connection* conn, types::iso15118::V2gMessageId message_id);

void report_din_request(const v2g_connection* conn, V2gMsgTypeId message_type, const V2gMsgReportContext& context);

void report_din_request(const v2g_connection* conn, V2gMsgTypeId message_type);

void report_din_response(const v2g_connection* conn, V2gMsgTypeId message_type,
                         V2gResReportContext<din_responseCodeType> context);

void report_din_response(const v2g_connection* conn, V2gMsgTypeId message_type);

void report_iso2_request(const v2g_connection* conn, V2gMsgTypeId message_type, const V2gMsgReportContext& context);

void report_iso2_request(const v2g_connection* conn, V2gMsgTypeId message_type);

void report_iso2_response(const v2g_connection* conn, V2gMsgTypeId message_type,
                          V2gResReportContext<iso2_responseCodeType> context);

void report_iso2_response(const v2g_connection* conn, V2gMsgTypeId message_type);

/**
 * @class Test
 * @brief The base class of V2G test case implementations.
 */
class Test : public virtual V2GServer {
public:
    // === Event Listeners ===
    virtual void on_test_started(v2g_connection* conn) {}
    virtual void on_test_finished(v2g_connection* conn) {}
    virtual void on_connection_close_event(v2g_connection* conn, const ConnectionCloseEvent& event) {}
    virtual void on_update_bsp_event(v2g_connection* conn, const UpdateBspEvent& event) {}
    virtual void on_powermeter_event(v2g_connection* conn, const PowermeterEvent& event) {}

    void stop_charging(v2g_context* ctx, bool& stop);
};


/**
 * @class DinTest
 * @brief A test case implementation of the DIN 70121 application protocol.
 */
class DinTest : public virtual Test, public virtual DinServerBase {
};


/**
 * @class Iso2Test
 * @brief A test case implementation of the ISO 15118-2 application protocol.
 */
class Iso2Test : public virtual Test, public virtual IsoServerBase {
};

} // namespace testing

#endif //TESTCASE_HPP
