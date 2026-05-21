<?php
include("config.php");
include("site_functions.php");
include("device_functions.php");


 
set_time_limit(0);
$host = '0.0.0.0';
$port = 8080;

$server = stream_socket_server(
    "tcp://$host:$port",
    $errno,
    $errstr
);

if (!$server) {
    die("Socket error: $errstr ($errno)\n");
}
stream_set_blocking($server, false);
echo "WebSocket server running on port $port\n";

/*
    CONNECTION STRUCTURE

    $connections[(int)$socket] = [
        'socket' => $socket,
        'type' => 'device' or 'frontend',
        'device_id' => 'abc123'
    ];
*/

$connections = [];

while (true) {
    $read = [$server];
    foreach ($connections as $conn) {
        $read[] = $conn['socket'];
    }
    $write = null;
    $except = null;
    if (stream_select($read, $write, $except, 1) === false) {
        continue;
    }

    /*
        NEW CONNECTION
    */

    if (in_array($server, $read, true)) {
        $client = @stream_socket_accept($server, 0);
        if ($client) {
            stream_set_blocking($client, false);
            $headers = fread($client, 2048);
            if (!$headers) {
                fclose($client);
                continue;
            }
            /*
                EXTRACT QUERY STRING
            */
            preg_match('#GET (.+?) HTTP#', $headers, $matches);
            $path = $matches[1] ?? '/';
            $parts = parse_url($path);
            parse_str($parts['query'] ?? '', $query);
            $deviceId = $query['device_id'] ?? null;
            $type = $query['type'] ?? null;
            if (!$deviceId || !$type) {
                fclose($client);
                continue;
            }
            /*
                WEBSOCKET HANDSHAKE
            */
            preg_match("/Sec-WebSocket-Key: (.*)\r\n/", $headers, $matches);

            $key = trim($matches[1]);

            $accept = base64_encode(
                pack(
                    'H*',
                    sha1(
                        $key .
                        '258EAFA5-E914-47DA-95CA-C5AB0DC85B11'
                    )
                )
            );
            $upgrade =
                "HTTP/1.1 101 Switching Protocols\r\n" .
                "Upgrade: websocket\r\n" .
                "Connection: Upgrade\r\n" .
                "Sec-WebSocket-Accept: $accept\r\n\r\n";

            fwrite($client, $upgrade);

            $id = (int)$client;

            $connections[$id] = [
                'socket' => $client,
                'type' => $type,
                'device_id' => $deviceId
            ];
            echo "CONNECTED: $type : $deviceId\n";
        }
        unset($read[array_search($server, $read, true)]);
    }

    /*
        HANDLE EXISTING CONNECTIONS
    */

    foreach ($read as $socket) {
        $id = (int)$socket;
        $data = @fread($socket, 2048);
        if (!$data) {
            if (isset($connections[$id])) {
                echo "DISCONNECTED: " .
                    $connections[$id]['type'] .
                    " : " .
                    $connections[$id]['device_id'] .
                    "\n";
                fclose($connections[$id]['socket']);
                unset($connections[$id]);
            }
            continue;
        }
        $decoded = decodeWebSocketFrame($data);
        if ($decoded === null) {
            continue;
        }
        $sender = $connections[$id];
        echo "MESSAGE FROM {$sender['type']} {$sender['device_id']}: $decoded\n";
        /*
            FORWARD TO MATCHING PEER
        */

        foreach ($connections as $targetId => $target) {
            if ($targetId === $id) {
                continue;
            }
            if (
                $target['device_id'] === $sender['device_id']
            ) {
                sendWebSocketMessage(
                    $target['socket'],
                    $decoded
                );
            }
        }
    }
}

/*
    DECODE INCOMING FRAME
*/

function decodeWebSocketFrame($data)
{
    $bytes = unpack('C*', $data);
    $length = $bytes[2] & 127;
    if ($length === 126) {
        $maskStart = 5;
        $payloadStart = 9;
    } elseif ($length === 127) {
        $maskStart = 11;
        $payloadStart = 15;

    } else {
        $maskStart = 3;
        $payloadStart = 7;
    }

    $mask = [
        $bytes[$maskStart],
        $bytes[$maskStart + 1],
        $bytes[$maskStart + 2],
        $bytes[$maskStart + 3]
    ];

    $payload = '';
    $j = 0;
    for ($i = $payloadStart; $i <= count($bytes); $i++) {
        $payload .= chr(
            $bytes[$i] ^ $mask[$j % 4]
        );
        $j++;
    }

    return $payload;
}

/*
    ENCODE OUTGOING FRAME
*/

function sendWebSocketMessage($client, $message)
{
    $length = strlen($message);
    $frame = chr(129);
    if ($length <= 125) {
        $frame .= chr($length);

    } elseif ($length <= 65535) {
        $frame .= chr(126) . pack("n", $length);
    } else {
        $frame .= chr(127) . pack("J", $length);
    }
    $frame .= $message;
    fwrite($client, $frame);
}

  