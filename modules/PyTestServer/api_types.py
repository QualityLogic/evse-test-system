from __future__ import annotations

__all__ = [
    'ApiEnum',
    'Scenario',
    'get_scenario_description',
    'ChargeMode',
    'IdentificationMode',
    'AppProtocol',
    'ScenarioConstraints',
]

from dataclasses import dataclass
from enum import Enum
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from typing import Any, Optional


class ApiEnum(Enum):

    def __new__(cls, api_id: str, mqtt_id: Any, display_name: str):
        obj = object.__new__(cls)
        obj._value_ = api_id
        obj.api_id = api_id
        obj.mqtt_id = mqtt_id
        obj.display_name = display_name
        return obj


class Scenario(ApiEnum):
    # CharIN Scenarios
    CHN_SESSION_SETUP_004 = 'chn_session_setup_004', 'V2GT_TC_CHN_SessionSetup_004', 'Session Setup 004'
    CHN_SESSION_SETUP_007 = 'chn_session_setup_007', 'V2GT_TC_CHN_SessionSetup_007', 'Session Setup 007'
    CHN_SERVICE_DISCOVERY_008 = 'chn_service_discovery_008', 'V2GT_TC_CHN_ServiceDiscovery_008', 'Service Discovery 008'
    CHN_SERVICE_DETAIL_011 = 'chn_service_detail_011', 'V2GT_TC_CHN_ServiceDetail_011', 'Service Detail 011'
    CHN_SERVICE_DETAIL_013 = 'chn_service_detail_013', 'V2GT_TC_CHN_ServiceDetail_013', 'Service Detail 013'
    CHN_SERVICE_DETAIL_AND_PAYMENT_SELECTION_003 = 'chn_service_detail_and_payment_selection_003', 'V2GT_TC_CHN_ServiceDetailAndPaymentSelection_003', 'Service Detail And Payment Selection 003'
    CHN_SERVICE_DETAIL_AND_PAYMENT_SELECTION_012 = 'chn_service_detail_and_payment_selection_012', 'V2GT_TC_CHN_ServiceDetailAndPaymentSelection_012', 'Service Detail And Payment Selection 012'
    CHN_AUTHORIZATION_004 = 'chn_authorization_004', 'V2GT_TC_CHN_Authorization_004', 'Authorization 004'
    CHN_AUTHORIZATION_009 = 'chn_authorization_009', 'V2GT_TC_CHN_Authorization_009', 'Authorization 009'
    CHN_CHARGE_PARAMETER_DISCOVERY_002 = 'chn_charge_parameter_discovery_002', 'V2GT_TC_CHN_ChargeParameterDiscovery_002', 'Charge Parameter Discovery 002'
    CHN_CABLE_CHECK_006 = 'chn_cable_check_006', 'V2GT_TC_CHN_CableCheck_006', 'Cable Check 006'
    CHN_CABLE_CHECK_007 = 'chn_cable_check_007', 'V2GT_TC_CHN_CableCheck_007', 'Cable Check 007'
    CHN_PRE_CHARGE_006 = 'chn_pre_charge_006', 'V2GT_TC_CHN_PreCharge_006', 'Pre-Charge 006'
    CHN_CURRENT_DEMAND_002 = 'chn_current_demand_002', 'V2GT_TC_CHN_CurrentDemand_002', 'Current Demand 002'
    CHN_CURRENT_DEMAND_005 = 'chn_current_demand_005', 'V2GT_TC_CHN_CurrentDemand_005', 'Current Demand 005'
    CHN_CURRENT_DEMAND_007 = 'chn_current_demand_007', 'V2GT_TC_CHN_CurrentDemand_007', 'Current Demand 007'
    CHN_WELDING_DETECTION_OR_SESSION_STOP_001 = 'chn_welding_detection_or_session_stop_001', 'V2GT_TC_CHN_WeldingDetectionOrSessionStop_001', 'Welding Detection Or Session Stop 001'
    # ChargeX Scenarios
    CHX_SMART_CHARGING_SCHEDULING_001 = 'chx_smart_charging_scheduling_001', 'V2GT_TC_CHX_SmartChargingScheduling_001', 'Smart Charging Scheduling 001'
    CHX_SMART_CHARGING_SCHEDULING_002 = 'chx_smart_charging_scheduling_002', 'V2GT_TC_CHX_SmartChargingScheduling_002', 'Smart Charging Scheduling 002'
    CHX_SMART_CHARGING_SCHEDULING_003 = 'chx_smart_charging_scheduling_003', 'V2GT_TC_CHX_SmartChargingScheduling_003', 'Smart Charging Scheduling 003'

    def get_short_name(self) -> str:
        short_name = self.display_name
        short_name = short_name.replace(' ', '')
        short_name = short_name.replace('-', '')
        return short_name


def get_scenario_description(scenario: Scenario) -> Optional[str]:
    match scenario:
        case Scenario.CHN_SESSION_SETUP_004:
            return ("Verifies that when no <code>SessionSetupRes</code> is sent after a valid <code>SessionSetupReq</code>,"
                    " the SUT correctly times out, transitions to CP State B (if not already applied), and terminates the"
                    " V2G TCP session.")
        case Scenario.CHN_SESSION_SETUP_007:
            return ("Verifies that when a <code>SessionSetupRes</code> (with OK_NewSessionEstablished) is sent after a"
                    " transition to <code>CP State F</code>, the SUT terminates the V2G TCP session.")
        case Scenario.CHN_SERVICE_DISCOVERY_008:
            return ("Verifies that when a <code>PaymentServiceSelectionRes</code> is sent instead of a <code>ServiceDiscoveryRes</code>"
                    ", the SUT transitions to CP State B (if not already applied), and terminates the V2G TCP session.")
        case Scenario.CHN_SERVICE_DETAIL_011:
            return ("Verifies that when no <code>ServiceDetailRes</code> is sent after a valid <code>ServiceDetailReq</code>,"
                    " the SUT correctly times out, transitions to CP State B (if not already applied), and terminates the"
                    " V2G TCP session.")
        case Scenario.CHN_SERVICE_DETAIL_013:
            return ("Verifies that when a <code>PaymentServiceSelectionRes</code> is sent instead of a <code>ServiceDetailRes</code>"
                    ", the SUT transitions to CP State B (if not already applied), and terminates the V2G TCP session.")
        case Scenario.CHN_SERVICE_DETAIL_AND_PAYMENT_SELECTION_003:
            return ("Verifies that when a <em>FAILED</em> <code>PaymentServiceSelectionRes</code> is sent with the oscillator"
                    " turned off, the SUT transitions to CP State B (if not already applied), and terminates the V2G TCP session.")
        case Scenario.CHN_SERVICE_DETAIL_AND_PAYMENT_SELECTION_012:
            return ("Verifies that when a <code>PaymentServiceSelectionRes</code> (with response code OK) is sent after a"
                    " transition to <code>CP State F</code>, the SUT terminates the V2G TCP session.")
        case Scenario.CHN_AUTHORIZATION_004:
            return ("Verifies that when a <em>FAILED_SequenceError</em> <code>AuthorizationRes</code> is sent with the oscillator"
                    " turned off, the SUT transitions to CP State B (if not already applied), and terminates the V2G TCP session.")
        case Scenario.CHN_AUTHORIZATION_009:
            return ("Verifies that when a <code>AuthorizationRes</code> (with <em>OK</em>, <em>Ongoing</em>) is continuously"
                    " sent, the SUT correctly times out, transitions to CP State B (if not already applied), and terminates"
                    " the V2G TCP session.")
        case Scenario.CHN_CHARGE_PARAMETER_DISCOVERY_002:
            return ("Verifies that when a <code>PaymentServiceSelectionRes</code> is sent instead of a <code>ChargeParameterDiscoveryRes</code>"
                    ", the SUT transitions to CP State B (if not already applied), and terminates the V2G TCP session.")
        case Scenario.CHN_CABLE_CHECK_006:
            return ("Verifies that when a <code>CableCheckRes</code> (with <em>OK</em>, <em>Ongoing</em>, <em>EVSE_IsolationMonitoringActive</em>)"
                    " is continuously sent, the SUT correctly times out, transitions to CP State B (if not already applied),"
                    " and terminates the V2G TCP session.")
        case Scenario.CHN_CABLE_CHECK_007:
            return ("Verifies that when a <code>PaymentServiceSelectionRes</code> is sent instead of a <code>CableCheckRes</code>"
                    ", the SUT transitions to CP State B (if not already applied), and terminates the V2G TCP session.")
        case Scenario.CHN_PRE_CHARGE_006:
            return ("Verifies that when a <code>PreChargeRes</code> is continuously sent with <code>EVSEPresentVoltage=0</code>,"
                    " the SUT correctly times out, transitions to CP State B (if not already applied), and terminates the"
                    " V2G TCP session.")
        case Scenario.CHN_CURRENT_DEMAND_002:
            return ("Verifies that when a <em>FAILED</em> <code>CurrentDemandRes</code> is sent with the oscillator"
                    " turned off, the SUT transitions to CP State B (if not already applied), and terminates the V2G TCP session.")
        case Scenario.CHN_CURRENT_DEMAND_005:
            return ("Verifies that when no <code>CurrentDemandRes</code> is sent after a valid <code>CurrentDemandReq</code>,"
                    " the SUT correctly times out, transitions to CP State B (if not already applied), and terminates the"
                    " V2G TCP session.")
        case Scenario.CHN_CURRENT_DEMAND_007:
            return ("Verifies that when a <code>CurrentDemandRes</code> (with response code <em>OK</em>) is sent after a"
                    " transition to <code>CP State F</code>, the SUT terminates the V2G TCP session.")
        case Scenario.CHN_WELDING_DETECTION_OR_SESSION_STOP_001:
            return ("Verifies that when a <code>PowerDeliveryRes</code> (with StopCharging) is sent, the SUT sends a"
                    " <code>SessionStopReq</code>, transitions to CP State B (if not already applied), and terminates the"
                    " V2G TCP session.")
        case Scenario.CHX_SMART_CHARGING_SCHEDULING_001:
            return "Verifies the SUT follows a charge schedule with 3-5 non-zero entries within its operating range."
        case Scenario.CHX_SMART_CHARGING_SCHEDULING_002:
            return ("Verifies the SUT follows a charge schedule with 3-5 non-zero entries where one entry besides the first"
                    " is 0A for 1 minute.")
        case Scenario.CHX_SMART_CHARGING_SCHEDULING_003:
            return ("Verifies the SUT follows a charge schedule with 3-5 non-zero entries where the first entry is 0A for"
                    " 1 minute.")
        case _:
            return None


class ChargeMode(ApiEnum):
    AC = 'ac', 0, 'AC'
    DC = 'dc', 1, 'DC'


class IdentificationMode(ApiEnum):
    EIM = 'eim', 0, 'External Identification Mode'
    PNC = 'pnc', 1, 'Plug & Charge'

    def get_short_name(self) -> str:
        match self:
            case IdentificationMode.EIM:
                return 'EIM'
            case IdentificationMode.PNC:
                return 'PnC'


class AppProtocol(ApiEnum):
    DIN_70121 = 'din_70121', 0, 'DIN 70121'
    ISO_15118_2 = 'iso_15118_2', 1, 'ISO 15118-2'


@dataclass
class ScenarioConstraints:
    app_protocol: AppProtocol
    charge_mode: ChargeMode
    ident_mode: IdentificationMode

    def to_mqtt_dict(self) -> dict[str, Any]:
        return {
            'protocols': [self.app_protocol.mqtt_id],
            'charge_modes': [self.charge_mode.mqtt_id],
            'ident_modes': [self.ident_mode.mqtt_id],
        }
