<?php

$file = __DIR__ . '/solarktestdata.txt';
$apcuKey = 'solark_line_offset';
$linesPerRequest = 40;

if (!is_readable($file)) {
    http_response_code(500);
    exit;
}

$offset = apcu_fetch($apcuKey);
if ($offset === false) {
    $offset = 0;
}

$fh = fopen($file, 'r');
if (!$fh) {
    http_response_code(500);
    exit;
}

fseek($fh, $offset);

$output = '';
$linesRead = 0;

while ($linesRead < $linesPerRequest) {
    $line = fgets($fh);

    if ($line === false) {
        // EOF: rewind and continue
        rewind($fh);
        $line = fgets($fh);
        if ($line === false) {
            break; // empty file safeguard
        }
    }

    $output .= $line;
    $linesRead++;
}

$offset = ftell($fh);
apcu_store($apcuKey, $offset);

fclose($fh);

echo $output;
