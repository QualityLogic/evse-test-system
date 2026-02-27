#  SPDX-License-Identifier: Apache-2.0
#  Copyright Pionix GmbH and Contributors to EVerest
import datetime
import json
import os
import threading
from dataclasses import dataclass, field
from http import HTTPStatus
from socketserver import ThreadingTCPServer
from tempfile import TemporaryDirectory
from typing import TypedDict
from urllib.parse import parse_qs, urlparse
from zipfile import ZipFile

from everest.framework import Module, RuntimeSession, log

from api_types import Scenario, AppProtocol, ChargeMode, IdentificationMode, ScenarioConstraints, get_scenario_description
from constraints import generate_constraints
from server import RequestHandler, RouteModule, api


class ModuleConfig(TypedDict):
    localhost: bool
    port: int
    payment_enable_eim: bool
    payment_enable_contract: bool
    charge_mode: str
    supported_DIN70121: bool
    supported_ISO15118_2: bool


class EverestServer(ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


@dataclass
class TrackedScenario:
    id: str
    scenario: Scenario
    constraints: ScenarioConstraints
    created_at: datetime.datetime = field(default_factory=datetime.datetime.utcnow)
    status: str = field(default='Queued')
    outcome: str = field(default=None)
    result: dict = field(default=None)

    def get_archive_name(self) -> str:
        try:
            timestamp = self.result['context']['start_timestamp']
        except KeyError:
            milliseconds = self.created_at.microsecond // 1000
            timestamp = self.created_at.strftime('%Y-%m-%dT%H:%M:%S.') + f'{milliseconds:03d}Z'

        name_parts = [timestamp]

        if self.constraints.app_protocol == AppProtocol.DIN_70121:
            name_parts.append('DIN')
        elif self.constraints.app_protocol == AppProtocol.ISO_15118_2:
            name_parts.append('ISO')

        name_parts.append(self.constraints.charge_mode.display_name)
        name_parts.append(self.constraints.ident_mode.get_short_name())
        name_parts.append(self.scenario.get_short_name())

        return '_'.join(name_parts)

    def to_creation_event(self) -> dict:
        return {
            'event': 'scenario-created',
            'id': self.id,
            'scenario': {'id': self.scenario.api_id, 'name': self.scenario.display_name},
            'protocol': {'id': self.constraints.app_protocol.api_id, 'name': self.constraints.app_protocol.display_name},
            'charge_mode': {'id': self.constraints.charge_mode.api_id, 'name': self.constraints.charge_mode.display_name},
            'ident_mode': {'id': self.constraints.ident_mode.api_id, 'name': self.constraints.ident_mode.get_short_name()},
            'created_at': self.created_at.isoformat(),
            'status': self.status,
            'outcome': self.outcome,
        }

    def to_deletion_event(self) -> dict:
        return {
            'event': 'scenario-deleted',
            'id': self.id,
        }

    def to_status_event(self) -> dict:
        return {
            'event': 'status-updated',
            'id': self.id,
            'status': self.status,
            'outcome': self.outcome,
        }

    def to_result_event(self) -> dict:
        return {
            'event': 'result-updated',
            'id': self.id,
            'outcome': self.outcome,
            'result': self.result,
            'status': self.status,
        }


class PyTestServerModule(RouteModule):

    def __init__(self) -> None:
        self._session = RuntimeSession()
        m = Module(self._session)

        log.update_process_name(m.info.id)

        self._setup = m.say_hello()

        # init ready event
        self._ready_event = threading.Event()

        self.tracked_scenarios = {}  # type: dict[str, TrackedScenario]
        self.event_connections = []  # type: list[RequestHandler]

        # setup connections
        m.subscribe_variable(
            self._setup.connections['test_manager'][0],
            'test_status',
            self.handle_test_status,
        )
        m.subscribe_variable(
            self._setup.connections['test_manager'][0],
            'test_result',
            self.handle_test_result,
        )

        self._mod = m
        self._mod.init_done(self._ready)

    def handle_test_status(self, data: dict):
        instance_id = data['run_id']  # unique identifier of the instance
        status = data['status']       # current status of the instance

        if scenario := self.tracked_scenarios.get(instance_id):
            scenario.status = status
            self.send_live_event(scenario.to_status_event())

    def handle_test_result(self, data: dict):
        instance_id = data['run_id']  # unique identifier of the instance
        outcome = data['outcome']     # overall outcome of the results

        if scenario := self.tracked_scenarios.get(instance_id):
            scenario.outcome = outcome
            scenario.result = data
            self.send_live_event(scenario.to_result_event())

    def call_enqueue_test(self, scenario: Scenario, constraints: ScenarioConstraints) -> str:
        module = self._setup.connections['test_manager'][0]
        result = self._mod.call_command(module, 'enqueue_test', {
            'test_id': scenario.mqtt_id,
            'constraints': constraints.to_mqtt_dict(),
        })
        return result

    @api.route('/api/v1/configuration', method='GET')
    def handle_get_configuration(self, http: RequestHandler) -> None:
        config = self._setup.configs.module  # type: ModuleConfig

        ac_supported = config['charge_mode'] == 'AC'
        dc_supported = config['charge_mode'] == 'DC'
        eim_supported = config['payment_enable_eim']
        pnc_supported = config['payment_enable_contract']
        din_supported = config['supported_DIN70121']
        iso_supported = config['supported_ISO15118_2']

        supported_app_protocols = []
        supported_charge_modes = []
        supported_ident_modes = []
        supported_scenarios = []

        if ac_supported: supported_charge_modes.append(ChargeMode.AC)
        if dc_supported: supported_charge_modes.append(ChargeMode.DC)
        if eim_supported: supported_ident_modes.append(IdentificationMode.EIM)
        if pnc_supported: supported_ident_modes.append(IdentificationMode.PNC)
        if din_supported: supported_app_protocols.append(AppProtocol.DIN_70121)
        if iso_supported: supported_app_protocols.append(AppProtocol.ISO_15118_2)

        for scenario in Scenario:
            constraints = generate_constraints(
                scenario=scenario,
                app_protocols=supported_app_protocols,
                charge_modes=supported_charge_modes,
                ident_modes=supported_ident_modes,
            )
            if len(constraints) > 0:
                supported_scenarios.append(scenario)

        app_protocol_entries = []
        charge_mode_entries = []
        ident_mode_entries = []
        scenario_entries = []

        for protocol in AppProtocol:
            app_protocol_entries.append({
                'id': protocol.api_id,
                'name': protocol.display_name,
                'disabled': len(supported_app_protocols) <= 1 or protocol not in supported_app_protocols,
                'default': len(supported_app_protocols) == 1 and supported_app_protocols[0] == protocol,
            })

        for charge_mode in ChargeMode:
            charge_mode_entries.append({
                'id': charge_mode.api_id,
                'name': charge_mode.display_name,
                'disabled': len(supported_charge_modes) <= 1 or charge_mode not in supported_ident_modes,
                'default': len(supported_charge_modes) == 1 and supported_charge_modes[0] == charge_mode,
            })

        for ident_mode in IdentificationMode:
            ident_mode_entries.append({
                'id': ident_mode.api_id,
                'name': ident_mode.display_name,
                'disabled': len(supported_ident_modes) <= 1 or ident_mode not in supported_ident_modes,
                'default': len(supported_ident_modes) == 1 and supported_ident_modes[0] == ident_mode,
            })

        for scenario in Scenario:
            scenario_entries.append({
                'id': scenario.api_id,
                'name': scenario.display_name,
                'description': get_scenario_description(scenario),
                'disabled': scenario not in supported_scenarios,
            })

        http.send_json({
            'charge_modes': charge_mode_entries,
            'identity': ident_mode_entries,
            'protocols': app_protocol_entries,
            'scenarios': scenario_entries,
        })

    @api.route('/api/v1/configuration', method='POST')
    def handle_upload_configuration(self, http: RequestHandler) -> None:
        data = http.read_json()

        try:
            charge_modes = list(map(ChargeMode, data['energy']))
            ident_modes = list(map(IdentificationMode, data['identity']))
            protocols = list(map(AppProtocol, data['protocols']))
            scenarios = list(map(Scenario, data['scenarios']))
        except (AttributeError, TypeError, ValueError) as error:
            http.send_error(HTTPStatus.BAD_REQUEST, str(error))
            return

        for scenario in scenarios:
            constraints = generate_constraints(
                scenario=scenario,
                app_protocols=protocols,
                charge_modes=charge_modes,
                ident_modes=ident_modes,
            )
            for constraint in constraints:
                instance_id = self.call_enqueue_test(scenario, constraint)
                tracked_scenario = TrackedScenario(
                    id=instance_id,
                    scenario=scenario,
                    constraints=constraint,
                )
                self.tracked_scenarios[instance_id] = tracked_scenario
                self.send_live_event(tracked_scenario.to_creation_event())

        http.send_response(HTTPStatus.OK)
        http.end_headers()

    @api.route('/api/v1/scenarios', method='GET')
    def handle_get_scenarios(self, http: RequestHandler) -> None:
        scenarios = sorted(self.tracked_scenarios.values(), key=lambda s: s.created_at)
        response = []

        for scenario in scenarios:
            data = scenario.to_creation_event()
            data['result'] = scenario.result
            del data['event']
            response.append(data)

        http.send_json(response)

    @api.route('/api/v1/events', method='GET')
    def handle_get_status(self, http: RequestHandler) -> None:
        http.send_response(HTTPStatus.OK)
        http.send_header('Content-Type', 'text/event-stream')
        http.send_header('Cache-Control', 'no-cache')
        http.send_header('Connection', 'keep-alive')
        http.end_headers()
        self.event_connections.append(http)

    @api.route('/api/v1/result', method='GET')
    def handle_download_results(self, http: RequestHandler) -> None:
        instance_id = None

        url = urlparse(http.path)
        params = parse_qs(url.query)
        if 'id' in params and len(params['id']) == 1:
            instance_id = params['id'][0]

        if instance_id and instance_id not in self.tracked_scenarios:
            http.send_error(HTTPStatus.NOT_FOUND, f"Scenario '{instance_id}' not found")
            return

        if instance_id and self.tracked_scenarios[instance_id].result is None:
            http.send_error(HTTPStatus.BAD_REQUEST, f"Scenario '{instance_id}' has no results")
            return

        with TemporaryDirectory() as tmpdir:
            if instance_id is not None:
                scenario = self.tracked_scenarios[instance_id]

                archive_name = scenario.get_archive_name() + '.zip'
                archive_path = os.path.join(tmpdir, archive_name)
                results_path = os.path.join(tmpdir, 'results.json')

                with open(results_path, mode='w') as f:
                    json.dump(scenario.result, f)

                with ZipFile(archive_path, mode='w') as zipf:
                    # Attach the results JSON file
                    zipf.write(results_path, os.path.basename(results_path))
                    # Attach the captured PCAP file
                    if pcap_file_path := scenario.result.get('pcap_file_path'):
                        zipf.write(pcap_file_path, 'traffic.pcap')

                http.send_file(archive_path, filetype='zip')
            else:
                now = datetime.datetime.utcnow()
                milliseconds = now.microsecond // 1000
                timestamp = now.strftime('%Y-%m-%dT%H:%M:%S.') + f'{milliseconds:03d}Z'

                archive_name = f'{timestamp}.results.zip'
                archive_path = os.path.join(tmpdir, archive_name)

                with ZipFile(archive_path, mode='w') as zipf:
                    for scenario in self.tracked_scenarios.values():
                        if not scenario.result:
                            continue

                        instance_name = scenario.get_archive_name()
                        instance_path = os.path.join(tmpdir, instance_name)
                        os.makedirs(instance_path, exist_ok=True)

                        results_name = 'results.json'
                        results_path = os.path.join(instance_path, results_name)

                        with open(results_path, mode='w') as f:
                            json.dump(scenario.result, f)

                        # Attach the results JSON file
                        zipf.write(results_path, os.path.join(instance_name, results_name))

                        # Attach the captured PCAP traffic
                        if pcap_file_path := scenario.result.get('pcap_file_path'):
                            zipf.write(pcap_file_path, os.path.join(instance_name, 'traffic.pcap'))

                http.send_file(archive_path, filetype='zip')

    @api.route('/api/v1/result', method='DELETE')
    def handle_delete_results(self, http: RequestHandler) -> None:
        instance_id = None

        url = urlparse(http.path)
        params = parse_qs(url.query)
        if 'id' in params and len(params['id']) == 1:
            instance_id = params['id'][0]

        if instance_id and instance_id not in self.tracked_scenarios:
            http.send_error(HTTPStatus.NOT_FOUND, f"Scenario '{instance_id}' not found")
            return

        if instance_id and self.tracked_scenarios[instance_id].result is None:
            http.send_error(HTTPStatus.BAD_REQUEST, f"Scenario '{instance_id}' has no results")
            return

        if instance_id is not None:
            scenario = self.tracked_scenarios[instance_id]

            # Delete the associated PCAP file
            if pcap_file := scenario.result.get('pcap_file_path'):
                if os.path.exists(pcap_file):
                    try:
                        log.debug(f"Removing '{pcap_file}'")
                        os.remove(pcap_file)
                    except OSError as error:
                        log.error(f"Error removing '{pcap_file}' : {error}")

            # Remove the tracked scenario
            log.debug(f"Removing tracked scenario '{instance_id}'")
            del self.tracked_scenarios[instance_id]

            self.send_live_event(scenario.to_deletion_event())
        else:
            for scenario in list(self.tracked_scenarios.values()):
                if not scenario.result:
                    continue

                    # Delete the associated PCAP file
                if pcap_file := scenario.result.get('pcap_file_path'):
                    if os.path.exists(pcap_file):
                        try:
                            log.debug(f"Removing '{pcap_file}'")
                            os.remove(pcap_file)
                        except OSError as error:
                            log.error(f"Error removing '{pcap_file}' : {error}")

                # Remove the tracked scenario
                log.debug(f"Removing tracked scenario '{scenario.id}'")
                del self.tracked_scenarios[scenario.id]

                self.send_live_event(scenario.to_deletion_event())

        http.send_response(HTTPStatus.OK)
        http.end_headers()

    def start_server(self) -> None:
        while True:
            self._ready_event.wait()

            try:
                config = self._setup.configs.module  # type: ModuleConfig

                host = ''
                port = config['port']

                if config['localhost']:
                    host = 'localhost'

                with EverestServer((host, port), RequestHandler) as httpd:
                    log.info(f'Everest server running on port {port}')
                    httpd.serve_forever()

            except KeyboardInterrupt:
                log.debug("SECC program terminated manually")
                break

            self._ready_event.clear()

    def _ready(self):
        self._ready_event.set()
        log.debug("ready!")

    #
    # Live Events
    #

    def send_live_event(self, event: dict) -> None:
        payload = b'data: ' + json.dumps(event).encode('utf-8') + b'\n\n'
        for connection in self.event_connections.copy():
            try:
                connection.wfile.write(payload)
            except OSError:
                self.event_connections.remove(connection)


py_test_server = PyTestServerModule()
py_test_server.start_server()
