<?php
//this is a PHP version of the parser used in the Arduino I2C Slave With Commands Repository:
//https://github.com/judasgutenberg/Arduino_I2C_Slave_With_Commands
//it uses configuration strings in the $css[] array to specify where values are found
//and a bitfield in $cis["PARSING_STYLE"] to control the details of what the values in the configuration strings mean
/* ============================================================
 * Compatibility helpers
 * ============================================================ */

if (!function_exists('str_starts_with')) {
    function str_starts_with(string $haystack, string $needle): bool {
        return $needle === '' || substr($haystack, 0, strlen($needle)) === $needle;
    }
}

/* ============================================================
 * Parsing-style bitfield
 * ============================================================ */

const PS_BIG_ENDIAN  = 0x04;
const PS_CHAR_OFFSET = 0x02;
const PS_ASCII_VALUE = 0x01;

$cis = [];
$cis['PARSING_STYLE'] = 0;   // known-good baseline

/* ============================================================
 * File + APCu
 * ============================================================ */

$file = __DIR__ . '/solarktestdata.txt';
$apcuKey = 'solark_line_offset';
$linesToScan = 22;

if (!is_readable($file)) {
    http_response_code(500);
    exit;
}

$offset = apcu_fetch($apcuKey);
if ($offset === false) $offset = 0;

/* ============================================================
 * Config blocks
 * ============================================================ */

$css = [];
$css[] = "Characteristic #2|0x3ffbb61c|0x3ffbb5fc|4|5|6|7|8|9|10|11|0x3ffbb60c|0|1";
$css[] = "Characteristic #7|I (318800102)|0x3ffbb5bc|6|7";

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

/* ============================================================
 * Offset index calculation
 * ============================================================ */

function calculateOffsetIndex(array $blocks, int $blkIndex, int $addrIndex): int
{
    $sum = 0;

    for ($b = 0; $b < $blkIndex; $b++) {
        foreach ($blocks[$b]['addrs'] as $aOffsets) {
            $sum += count($aOffsets) >> 1;
        }
    }

    $addrs = array_values($blocks[$blkIndex]['addrs']);
    for ($a = 0; $a < $addrIndex; $a++) {
        $sum += count($addrs[$a]) >> 1;
    }

    return $sum;
}

/* ============================================================
 * Parsing helpers
 * ============================================================ */

function findNthToken(string $s, int $n): ?int
{
    $tokens = preg_split('/\s+/', trim($s));
    if (!isset($tokens[$n])) return null;

    $pos = strpos($s, $tokens[$n]);
    return $pos === false ? null : $pos;
}

function hexByteAt(string $s, int $pos): ?int
{
    if (!isset($s[$pos + 1])) return null;

    $pair = substr($s, $pos, 2);
    if (!ctype_xdigit($pair)) return null;

    return hexdec($pair);
}

function readValueAtOffset(
    string $line,
    string $addr,
    int $off1,
    int $off2,
    int $parsingStyle,
    ?int &$outValue
): bool {

    $addrPos = strpos($line, $addr);
    if ($addrPos === false) return false;

    $addrPos += strlen($addr);
    $substr = substr($line, $addrPos);

    /* ---- OFFSET RESOLUTION ---- */

    if ($parsingStyle & PS_CHAR_OFFSET) {
        $p1 = $addrPos + $off1;
        $p2 = $addrPos + $off2;
    } else {
        $t1 = findNthToken($substr, $off1);
        if ($t1 === null) return false;

        $t2 = ($off2 !== $off1) ? findNthToken($substr, $off2) : $t1;

        $p1 = $addrPos + $t1;
        $p2 = $addrPos + ($t2 ?? $t1);
    }

    if (!isset($line[$p1])) return false;

    /* ---- VALUE INTERPRETATION ---- */

    if ($parsingStyle & PS_ASCII_VALUE) {
        $b0 = ord($line[$p1]);
        $b1 = ($off1 !== $off2 && isset($line[$p2])) ? ord($line[$p2]) : 0;
    } else {
        $b0 = hexByteAt($line, $p1);
        if ($b0 === null) return false;

        if ($off1 !== $off2) {
            $b1 = hexByteAt($line, $p2);
            if ($b1 === null) return false;
        } else {
            $b1 = 0;
        }
    }

    /* ---- ENDIANNESS ---- */

    if ($off1 === $off2) {
        $outValue = $b0;
    } elseif ($parsingStyle & PS_BIG_ENDIAN) {
        $outValue = ($b0 << 8) | $b1;
    } else {
        $outValue = ($b1 << 8) | $b0;
    }

    return true;
}

/* ============================================================
 * Packet initialization
 * ============================================================ */

$packetWordCount = 64;
$packet = array_fill(0, $packetWordCount, 0xFFFF);

/* ============================================================
 * Main parse loop
 * ============================================================ */

$fh = fopen($file, 'r');
fseek($fh, $offset);

$linesRead = 0;
unset($activeBlock);

while ($linesRead++ < $linesToScan && ($line = fgets($fh)) !== false) {

    foreach ($config as $blkIndex => $blk) {

        if (strpos($line, $blk['start']) !== false) {
            $activeBlock = $blkIndex;
            continue 2;
        }

        if (!isset($activeBlock)) continue;

        if (strpos($line, $blk['end']) !== false) {
            unset($activeBlock);
            continue 2;
        }

        $addrKeys = array_keys($blk['addrs']);

        foreach ($addrKeys as $addrIndex => $addr) {

            if (strpos($line, $addr) === false) continue;

            $baseIndex = calculateOffsetIndex($config, $blkIndex, $addrIndex);
            $offsets   = $blk['addrs'][$addr];

            for ($i = 0; $i + 1 < count($offsets); $i += 2) {

                $val = null;

                if (!readValueAtOffset(
                        $line,
                        $addr,
                        $offsets[$i],
                        $offsets[$i + 1],
                        $cis['PARSING_STYLE'],
                        $val))
                {
                    $packet[$baseIndex + ($i >> 1)] = 0xFFFF;
                } else {
                    $packet[$baseIndex + ($i >> 1)] = $val;
                }
            }

            break;
        }
    }
}

/* ============================================================
 * Convert packet to byte string (LE)
 * ============================================================ */

$bytesOut = '';
foreach ($packet as $w) {
    $bytesOut .= chr($w & 0xFF) . chr(($w >> 8) & 0xFF);
}

/* ============================================================
 * Output + debug
 * ============================================================ */

echo $bytesOut;

$parsedBytes = array_map('ord', str_split($bytesOut));

echo "<br/>HEX:<br/>";
foreach ($parsedBytes as $b) printf('%02X ', $b);

echo "<br/><br/>U16 DEC (LE):<br/>";
for ($i = 0; $i + 1 < count($parsedBytes); $i += 2) {
    echo ($parsedBytes[$i] | ($parsedBytes[$i + 1] << 8)) . ' ';
}

echo "<br/>";
