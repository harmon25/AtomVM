%
% This file is part of AtomVM.
%
% Copyright 2024 Paul Guyot <pguyot@kallisys.net>
%
% Licensed under the Apache License, Version 2.0 (the "License");
% you may not use this file except in compliance with the License.
% You may obtain a copy of the License at
%
%    http://www.apache.org/licenses/LICENSE-2.0
%
% Unless required by applicable law or agreed to in writing, software
% distributed under the License is distributed on an "AS IS" BASIS,
% WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
% See the License for the specific language governing permissions and
% limitations under the License.
%
% SPDX-License-Identifier: Apache-2.0 OR LGPL-2.1-or-later
%

-module(wifi_scan_example).

-export([start/0]).

start() ->
    io:format("WiFi Scan Example~n"),
    
    % Initialize network in STA mode (required for scanning)
    % We don't need to connect, just initialize the WiFi
    Config = [{sta, []}],
    
    case network:start(Config) of
        {ok, _Pid} ->
            io:format("Network initialized, starting scan...~n"),
            timer:sleep(1000), % Give WiFi time to initialize
            
            case network:wifi_scan() of
                {ok, AccessPoints} ->
                    io:format("Found ~p access points:~n", [length(AccessPoints)]),
                    lists:foreach(fun print_ap/1, AccessPoints),
                    network:stop(),
                    ok;
                {error, Reason} ->
                    io:format("Scan failed: ~p~n", [Reason]),
                    network:stop(),
                    error
            end;
        {error, Reason} ->
            io:format("Failed to start network: ~p~n", [Reason]),
            error
    end.

print_ap(AP) ->
    SSID = maps:get(ssid, AP, <<>>),
    BSSID = maps:get(bssid, AP, <<>>),
    RSSI = maps:get(rssi, AP, 0),
    Channel = maps:get(channel, AP, 0),
    AuthMode = maps:get(authmode, AP, unknown),
    
    % Format BSSID as MAC address
    BSSIDStr = format_mac(BSSID),
    
    io:format("  SSID: ~s~n", [SSID]),
    io:format("    BSSID: ~s~n", [BSSIDStr]),
    io:format("    RSSI: ~p dBm~n", [RSSI]),
    io:format("    Channel: ~p~n", [Channel]),
    io:format("    Auth: ~p~n~n", [AuthMode]).

format_mac(<<A, B, C, D, E, F>>) ->
    io_lib:format("~2.16.0B:~2.16.0B:~2.16.0B:~2.16.0B:~2.16.0B:~2.16.0B", [A, B, C, D, E, F]);
format_mac(_) ->
    "unknown".
