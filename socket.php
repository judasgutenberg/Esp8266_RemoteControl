<?php
include("config.php");
include("site_functions.php");
include("device_functions.php");

$server = stream_socket_server("tcp://0.0.0.0:8080", $errno, $errstr);
$outData = [];

if (!$server) {
    die("$errstr ($errno)\n");
}

echo "WebSocket server started\n";

$clients = [];

while (true) {
  $read = [];

  // only include valid client sockets
  foreach ($clients as $client) {

      if (is_resource($client)) {
          $read[] = $client;
      }
  }

  // include master server socket
  if (is_resource($server)) {
      $read[] = $server;
  }

  // nothing valid left
  if (empty($read)) {
      usleep(100000);
      continue;
  }

  $write = null;
  $except = null;

  $result = @stream_select($read, $write, $except, null);

  if ($result === false) {
      echo "stream_select failed\n";
      continue;
  }

  // --------------------------------------------------
  // NEW CONNECTIONS
  // --------------------------------------------------

  if (in_array($server, $read, true)) {

      $client = @stream_socket_accept($server, 0);

      if ($client) {

          $headers = fread($client, 1500);

          preg_match("/Sec-WebSocket-Key: (.*)\r\n/", $headers, $matches);

          if (!empty($matches[1])) {

              $key = trim($matches[1]);

              $accept = base64_encode(
                  pack(
                      'H*',
                      sha1($key . '258EAFA5-E914-47DA-95CA-C5AB0DC85B11')
                  )
              );

              $upgrade =
                  "HTTP/1.1 101 Switching Protocols\r\n" .
                  "Upgrade: websocket\r\n" .
                  "Connection: Upgrade\r\n" .
                  "Sec-WebSocket-Accept: $accept\r\n\r\n";

              fwrite($client, $upgrade);

              stream_set_blocking($client, false);

              $clients[] = $client;

              echo "client connected\n";
          }
          else {

              fclose($client);

              echo "invalid websocket handshake\n";
          }
      }

      // remove server socket from readable clients
      $serverKey = array_search($server, $read, true);

      if ($serverKey !== false) {
          unset($read[$serverKey]);
      }
  }

  // --------------------------------------------------
  // EXISTING CLIENT DATA
  // --------------------------------------------------

  foreach ($read as $client) {

      if (!is_resource($client)) {
          continue;
      }

      $data = @fread($client, 2048);

      if ($data === '' || $data === false) {

          echo "client disconnected\n";

          $key = array_search($client, $clients, true);

          if ($key !== false) {
              unset($clients[$key]);
          }

          fclose($client);

          continue;
      }

      $payload = decodeFrame($data);

      if ($payload !== false && trim($payload) !== '') {

          echo "received: $payload\n";
          if(substr($payload, 0, 1) == "{") {
             $data = json_decode($payload, true);
             if($data) {
                $specificDeviceId = gvfa("device_id", $data);
                if($specificDeviceId > 0) {
                  if(!array_key_exists($specificDeviceId , $outData)) 
                   $outData[$specificDeviceId] = "";
                   }
                }
             }
            
            

          $message = encodeFrame("server received: $payload");

          foreach ($clients as $sendClient) {

              if (is_resource($sendClient)) {
                  @fwrite($sendClient, $message);
              }
          }
      }
  }
}

function encodeFrame($payload)
{
    $frame = [];
    $frame[0] = 129;
    $length = strlen($payload);
    if ($length <= 125) {
        $frame[1] = $length;
    } else {
      $frame[1] = 0;
    }
    $header = pack('CC', $frame[0], $frame[1]);
    return $header . $payload;
}


function decodeFrame($data)
{
    $opcode = ord($data[0]) & 0x0F;

    // only process text frames
    if ($opcode !== 0x1) {
        return false;
    }

    $length = ord($data[1]) & 127;

    if ($length == 126) {
        $masks = substr($data, 4, 4);
        $payload = substr($data, 8);
    }
    elseif ($length == 127) {
        $masks = substr($data, 10, 4);
        $payload = substr($data, 14);
    }
    else {
        $masks = substr($data, 2, 4);
        $payload = substr($data, 6);
    }

    $text = '';

    for ($i = 0; $i < strlen($payload); $i++) {
        $text .= $payload[$i] ^ $masks[$i % 4];
    }

    return $text;
}

  