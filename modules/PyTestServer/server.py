#  SPDX-License-Identifier: Apache-2.0
#  Copyright Pionix GmbH and Contributors to EVerest
from __future__ import annotations

__all__ = [
    'API',
    'RequestHandler',
    'Route',
    'RouteModule',
    'RouteModuleMeta',
    'api',
]

import json
import os.path
import traceback
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler
from typing import TYPE_CHECKING, Generic, TypeVar
from urllib.parse import urlparse

if TYPE_CHECKING:
    from collections.abc import Iterable
    from typing import Any, Callable, Literal, Optional, TypedDict, Union
    from typing_extensions import Concatenate, ParamSpec, Self, Unpack

    Method = Literal['GET', 'POST', 'PUT', 'PATCH', 'DELETE']

    class _RouteKwargs(TypedDict, total=False):
        methods: Iterable[Method]
        path: str


M = TypeVar('M')
T = TypeVar('T')

if TYPE_CHECKING:
    P = ParamSpec('P')
else:
    P = TypeVar('P')


class Route(Generic[M, P, T]):

    def __init__(
        self,
        func: Union[
            Callable[Concatenate[M, RequestHandler, P], T],
            Callable[Concatenate[RequestHandler, P], T],
        ],
        /,
        **kwargs: Unpack[_RouteKwargs],
    ) -> None:
        self.methods = tuple(kwargs.pop('methods', []))  # type: tuple[Method, ...]
        self.path = kwargs.pop('path', '/')  # type: str
        self._callback = func
        self._module = None

    @property
    def callback(
        self,
    ) -> Union[
        Callable[Concatenate[M, RequestHandler, P], T],
        Callable[Concatenate[RequestHandler, P], T],
    ]:
        return self._callback

    @callback.setter
    def callback(
        self,
        func: Union[
            Callable[Concatenate[M, RequestHandler, P], T],
            Callable[Concatenate[RequestHandler, P], T],
        ],
    ) -> None:
        self._callback = func

    @property
    def module(self) -> Optional[M]:
        return self._module

    @module.setter
    def module(self, value: Optional[M]) -> None:
        self._module = value

    def __call__(self, http: RequestHandler, /, *args: P.args, **kwargs: P.kwargs) -> T:
        if self.module is not None:
            return self.callback(self.module, http, *args, **kwargs)
        else:
            return self.callback(http, *args, **kwargs)


class API:

    def __init__(self) -> None:
        self.routes: dict[str, dict[Method, Route]] = {}

    def add_route(self, route: Route) -> None:
        if route.path not in self.routes:
            self.routes[route.path] = {}

        for method in route.methods:
            if method in self.routes[route.path]:
                raise ValueError(f"Method {method} already exists for route '{route.path}'")

            self.routes[route.path][method] = route

    def remove_route(self, route: Route) -> None:
        if route_methods := self.routes.get(route.path):
            for method in route.methods:
                if method in route_methods:
                    del route_methods[method]
            if not route_methods:
                del self.routes[route.path]

    def has_route(self, path: str) -> bool:
        return path in self.routes

    def get_route(self, path: str, method: Method) -> Optional[Route]:
        if route_methods := self.routes.get(path):
            return route_methods.get(method)
        return None

    def route(self, path: str, /, *, method: Method = None, methods: Iterable[Method] = None):

        full_methods = set(filter(None, [method] + (methods or [])))

        def decorator(
            func: Union[
                Callable[Concatenate[M, RequestHandler, P], T],
                Callable[Concatenate[RequestHandler, P], T],
            ],
        ) -> Route[M, P, T]:
            route = Route(func, path=path, methods=full_methods)
            self.add_route(route)
            return route

        return decorator


api = API()


class RouteModuleMeta(type):
    __api_routes__: list[Route[Any, ..., Any]]

    def __new__(cls, *args: Any, **kwargs: Any) -> RouteModuleMeta:
        api_routes = {}
        new_cls = super().__new__(cls, *args, **kwargs)

        for base in reversed(new_cls.__mro__):
            for elem, value in base.__dict__.items():

                if elem in api_routes:
                    del api_routes[elem]

                is_static_method = isinstance(value, staticmethod)
                if is_static_method:
                    value = value.__func__

                if isinstance(value, Route):
                    if is_static_method:
                        raise TypeError(f'Route in method {base}.{elem!r} must not be staticmethod.')
                    api_routes[elem] = value

        new_cls.__api_routes__ = list(api_routes.values())

        return new_cls

    def __init__(cls, *args: Any, **kwargs: Any) -> None:
        super().__init__(*args)


class RouteModule(metaclass=RouteModuleMeta):
    __api_routes__: list[Route[Self, ..., Any]]

    def __new__(cls, *args: Any, **kwargs: Any) -> Self:
        self = super().__new__(cls)

        for route in self.__api_routes__:
            route.module = self

        return self

    def get_api_routes(self) -> list[Route[Self, ..., Any]]:
        return list(self.__api_routes__)


class InternalError(Exception):
    def __init__(self, handled: bool) -> None:
        self.handled = handled


# noinspection PyPep8Naming
class RequestHandler(SimpleHTTPRequestHandler):
    global api

    def __init__(self, *args: Any, **kwargs: Any) -> None:
        web_dir = os.path.dirname(__file__)
        web_dir = os.path.join(web_dir, 'web')

        super().__init__(*args, directory=web_dir, **kwargs)

        self.method: Optional[Method] = None

    def log_request(self, code = "-", size = "-"):
        try:
            if not (200 <= int(code) < 300):
                super().log_request(code, size)
        except ValueError:
            super().log_request(code, size)

    #
    # Route Handlers
    #

    def do_GET(self) -> None:
        path = urlparse(self.path).path

        if not api.has_route(path):
            super().do_GET()
        else:
            self._call_api('GET')

    def do_POST(self) -> None:
        self._call_api('POST')

    def do_PUT(self) -> None:
        self._call_api('PUT')

    def do_PATCH(self) -> None:
        self._call_api('PATCH')

    def do_DELETE(self) -> None:
        self._call_api('DELETE')

    def _call_api(self, method: Method) -> None:
        try:
            path = urlparse(self.path).path

            if route := api.get_route(path, method):
                self.method = method
                route(self)
            elif api.has_route(path):
                self.send_error(HTTPStatus.METHOD_NOT_ALLOWED)
            else:
                self.send_error(HTTPStatus.NOT_FOUND)

        except Exception as error:
            if isinstance(error, InternalError) and error.handled:
                return

            traceback.print_exc()
            self.send_error(HTTPStatus.INTERNAL_SERVER_ERROR)

    #
    # Request Handlers
    #

    def read_json(self) -> Any:
        try:
            content_length = int(self.headers.get('Content-Length', 0))
            content_type = str(self.headers.get('Content-Type', ''))
        except (ValueError, TypeError) as error:
            self.send_error(HTTPStatus.BAD_REQUEST)
            raise InternalError(handled=True) from error

        if content_type != 'application/json':
            self.send_error(HTTPStatus.BAD_REQUEST, 'Posted data must be in JSON format')
            raise InternalError(handled=True)

        payload = self.rfile.read(content_length)

        try:
            return json.loads(payload.decode('utf-8'))
        except (UnicodeDecodeError, json.decoder.JSONDecodeError) as error:
            self.send_error(HTTPStatus.BAD_REQUEST, 'Posted data must be in JSON format')
            raise InternalError(handled=True) from error

    #
    # Response Builders
    #

    def send_file(
        self,
        path: str, *,
        filename: str = None,
        filetype: str = 'octet-stream',
        chunklen: int = 4096,
    ) -> None:
        """
        Send a file's contents as a stream of chunks.

        :param path:     Path to the file to send.
        :param filename: Alternative file name to use instead of the original.
        :param filetype: Content type of the file. Default is 'octet-stream'.
        :param chunklen: Number of bytes to send at a time. Default is 4096.
        :raises OSError: If there are problems reading or sending the file.
        """
        if filename is None:
            filename = os.path.basename(path)

        with open(path, 'rb') as file:
            self.send_response(HTTPStatus.OK)
            self.send_header('Content-Type', f'application/{filetype}')
            self.send_header('Content-Disposition', f'attachment; filename="{filename}"')
            # self.send_header('Transfer-Encoding', 'chunked')
            self.end_headers()

            while True:
                chunk = file.read(chunklen)
                if not chunk: break
                self.wfile.write(chunk)

    def send_json(self, obj: Any, *, indent: int = None) -> None:
        """
        Serialize and send an object as JSON.

        :param obj:    The object to serialize as JSON.
        :param indent: The indentation level to pretty-print the JSON with.
        """
        self.send_response(HTTPStatus.OK)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(obj, indent=indent).encode('utf-8'))
