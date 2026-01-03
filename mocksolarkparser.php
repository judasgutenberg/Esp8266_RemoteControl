<?php

$file = __DIR__ . '/solarktestdata.txt';
$apcuKey = 'solark_line_offset';
$linesToScan = 200;   // how far ahead we look


if (!function_exists('str_starts_with')) {
    function str_starts_with(string $haystack, string $needle): bool {
        return $needle === '' || substr($haystack, 0, strlen($needle)) === $needle;
    }
}

if (!is_readable($file)) {
    http_response_code(500);
    exit;
}

$offset = apcu_fetch($apcuKey);
if ($offset === false) {
    $offset = 0;
}

/* -------------------------------
   Config strings
-------------------------------- */

$css = [];

$css[] = "Characteristic #2|0x3ffbb61c|0x3ffbb5fc|4|5|6|7|8|9|10|11|0x3ffbb60c|0|1";
$css[] = "Characteristic #7|I (318800102)|0x3ffbb5bc|6|7";

/* -------------------------------
   Parse config strings
-------------------------------- */

$config = [];

foreach ($css as $cfg) {
    $parts = explode('|', $cfg);

    $block = [
        'start' => array_shift($parts),
        'end'   => array_shift($parts),
        'addrs' => []
    ];

    $curAddr = null;

    foreach ($parts as $p) {
        if (str_starts_with($p, '0x') || $p[0] === 'I') {
            $curAddr = $p;
            $block['addrs'][$curAddr] = [];
        } else {
            $block['addrs'][$curAddr][] = (int)$p;
        }
    }

    $config[] = $block;
}

/* -------------------------------
   Open stream snapshot
-------------------------------- */

$fh = fopen($file, 'r');
fseek($fh, $offset);

$activeBlock = null;
$bytesOut = '';

/* -------------------------------
   Scan stream
-------------------------------- */

$linesRead = 0;

while ($linesRead++ < $linesToScan && ($line = fgets($fh)) !== false) {

    // Handle EOF wrap
    if ($line === false) {
        rewind($fh);
        continue;
    }

    // Detect block start
    foreach ($config as $i => $blk) {
        if (strpos($line, $blk['start']) !== false) {
            $activeBlock = $i;
            $bytesOut = '';
            continue 2;
        }
    }

    if ($activeBlock === null) {
        continue;
    }

    // Detect block end
    if (strpos($line, $config[$activeBlock]['end']) !== false) {
        break;
    }

    // Address processing
    foreach ($config[$activeBlock]['addrs'] as $addr => $offsets) {
        if (strpos($line, $addr) === false) {
            continue;
        }

        if (!preg_match_all('/\b[0-9a-fA-F]{2}\b/', $line, $m)) {
            continue;
        }

        $bytes = array_map('hexdec', $m[0]);

        foreach ($offsets as $off) {
            if (!isset($bytes[$off])) {
                continue;
            }

            $v = $bytes[$off] & 0xFF;

            // little-endian uint16
            $bytesOut .= chr($v);
            $bytesOut .= chr(0x00);
        }
    }
}

fclose($fh);

/* -------------------------------
   Output binary blob
-------------------------------- */

//header('Content-Type: application/octet-stream');
//header('Content-Length: ' . strlen($bytesOut));
echo $bytesOut;
$parsedBytes = array_map('ord', str_split($bytesOut));
echo "<br/>\n\nHEX:\n";
foreach ($parsedBytes as $b) {
    printf('%02X ', $b);
}

echo "<br/>\n\nDEC:\n";
foreach ($parsedBytes as $b) {
    echo $b . ' ';
}
echo "<br/>\n\nU16 DEC (LE):\n";
for ($i = 0; $i + 1 < count($parsedBytes); $i += 2) {
    $val = $parsedBytes[$i] | ($parsedBytes[$i + 1] << 8);
    echo $val . ' ';
}
echo "\n";