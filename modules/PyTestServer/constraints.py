from __future__ import annotations

__all__ = [
    'generate_constraints',
]

from dataclasses import dataclass
from typing import TYPE_CHECKING

from api_types import (
    Scenario,
    AppProtocol,
    ChargeMode,
    IdentificationMode,
    ScenarioConstraints,
)

if TYPE_CHECKING:
    from typing import Optional
    from collections.abc import Sequence


@dataclass(frozen=True)
class _ScenarioConstraint:
    app_protocols: set[AppProtocol]
    charge_modes: set[ChargeMode]
    ident_modes: set[IdentificationMode]


@dataclass(frozen=True)
class _ChargeModeConstraint:
    app_protocols: set[AppProtocol]


@dataclass(frozen=True)
class _AppProtocolConstraint:
    ident_modes: set[IdentificationMode]


def get_scenario_constraints(scenario: Scenario) -> Optional[_ScenarioConstraint]:
    match scenario:
        case Scenario.CHN_SESSION_SETUP_004:
            return _ScenarioConstraint(app_protocols={AppProtocol.DIN_70121, AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.AC, ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case Scenario.CHN_SESSION_SETUP_007:
            return _ScenarioConstraint(app_protocols={AppProtocol.DIN_70121, AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.AC, ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case Scenario.CHN_SERVICE_DISCOVERY_008:
            return _ScenarioConstraint(app_protocols={AppProtocol.DIN_70121, AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.AC, ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case Scenario.CHN_SERVICE_DETAIL_011:
            return _ScenarioConstraint(app_protocols={AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.AC, ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case Scenario.CHN_SERVICE_DETAIL_013:
            return _ScenarioConstraint(app_protocols={AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.AC, ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case Scenario.CHN_SERVICE_DETAIL_AND_PAYMENT_SELECTION_003:
            return _ScenarioConstraint(app_protocols={AppProtocol.DIN_70121, AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.AC, ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case Scenario.CHN_SERVICE_DETAIL_AND_PAYMENT_SELECTION_012:
            return _ScenarioConstraint(app_protocols={AppProtocol.DIN_70121, AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.AC, ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case Scenario.CHN_AUTHORIZATION_004:
            return _ScenarioConstraint(app_protocols={AppProtocol.DIN_70121, AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.AC, ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case Scenario.CHN_AUTHORIZATION_009:
            return _ScenarioConstraint(app_protocols={AppProtocol.DIN_70121},
                                       charge_modes={ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM})
        case Scenario.CHN_CHARGE_PARAMETER_DISCOVERY_002:
            return _ScenarioConstraint(app_protocols={AppProtocol.DIN_70121, AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.AC, ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case Scenario.CHN_CABLE_CHECK_006:
            return _ScenarioConstraint(app_protocols={AppProtocol.DIN_70121, AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case Scenario.CHN_CABLE_CHECK_007:
            return _ScenarioConstraint(app_protocols={AppProtocol.DIN_70121, AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case Scenario.CHN_PRE_CHARGE_006:
            return _ScenarioConstraint(app_protocols={AppProtocol.DIN_70121, AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case Scenario.CHN_CURRENT_DEMAND_002:
            return _ScenarioConstraint(app_protocols={AppProtocol.DIN_70121, AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case Scenario.CHN_CURRENT_DEMAND_005:
            return _ScenarioConstraint(app_protocols={AppProtocol.DIN_70121, AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case Scenario.CHN_CURRENT_DEMAND_007:
            return _ScenarioConstraint(app_protocols={AppProtocol.DIN_70121, AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case Scenario.CHN_WELDING_DETECTION_OR_SESSION_STOP_001:
            return _ScenarioConstraint(app_protocols={AppProtocol.DIN_70121, AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.AC, ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case Scenario.CHX_SMART_CHARGING_SCHEDULING_001:
            return _ScenarioConstraint(app_protocols={AppProtocol.DIN_70121, AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.AC, ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case Scenario.CHX_SMART_CHARGING_SCHEDULING_002:
            return _ScenarioConstraint(app_protocols={AppProtocol.DIN_70121, AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.AC, ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case Scenario.CHX_SMART_CHARGING_SCHEDULING_003:
            return _ScenarioConstraint(app_protocols={AppProtocol.DIN_70121, AppProtocol.ISO_15118_2},
                                       charge_modes={ChargeMode.AC, ChargeMode.DC},
                                       ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case _:
            return None


def get_charge_mode_constraints(charge_mode: ChargeMode) -> Optional[_ChargeModeConstraint]:
    match charge_mode:
        case ChargeMode.AC:
            return _ChargeModeConstraint(app_protocols={AppProtocol.ISO_15118_2})
        case ChargeMode.DC:
            return _ChargeModeConstraint(app_protocols={AppProtocol.DIN_70121, AppProtocol.ISO_15118_2})
        case _:
            return None


def get_app_protocol_constraints(protocol: AppProtocol) -> Optional[_AppProtocolConstraint]:
    match protocol:
        case AppProtocol.DIN_70121:
            return _AppProtocolConstraint(ident_modes={IdentificationMode.EIM})
        case AppProtocol.ISO_15118_2:
            return _AppProtocolConstraint(ident_modes={IdentificationMode.EIM, IdentificationMode.PNC})
        case _:
            return None


def generate_constraints(
    scenario: Scenario,
    app_protocols: Sequence[AppProtocol],
    charge_modes: Sequence[ChargeMode],
    ident_modes: Sequence[IdentificationMode],
) -> list[ScenarioConstraints]:
    constraints = []

    if scenario_constraint := get_scenario_constraints(scenario):
        app_protocols = scenario_constraint.app_protocols.intersection(app_protocols)
        ident_modes = scenario_constraint.ident_modes.intersection(ident_modes)

        for charge_mode in scenario_constraint.charge_modes.intersection(charge_modes):
            if charge_mode_constraints := get_charge_mode_constraints(charge_mode):

                for protocol in charge_mode_constraints.app_protocols.intersection(app_protocols):
                    if protocol_constraints := get_app_protocol_constraints(protocol):

                        for ident_mode in protocol_constraints.ident_modes.intersection(ident_modes):
                            constraint = ScenarioConstraints(app_protocol=protocol,
                                                             charge_mode=charge_mode,
                                                             ident_mode=ident_mode)
                            constraints.append(constraint)

    return constraints
