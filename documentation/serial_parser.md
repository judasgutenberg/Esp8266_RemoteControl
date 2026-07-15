# Serial String Parser

A configuration-driven serial parser for embedded systems. Rather than
writing device-specific parsing code, the parser is driven entirely by
`serialparser.cfg`. New protocols can usually be supported by changing
configuration rather than recompiling firmware.

## Design Goals

-   Zero device-specific parsing logic.
-   Small RAM footprint.
-   Stream processing of serial input.
-   Configuration determines packet layout.
-   Support multiple serial formats through parser mode flags.

------------------------------------------------------------------------

# How It Works

    Serial Stream
          │
          ▼
    Read complete line
          │
          ▼
    Search for block start
          │
          ▼
    Activate parsing block
          │
          ▼
    Find configured address strings
          │
          ▼
    Extract bytes using offset rules
          │
          ▼
    Convert to 16-bit values
          │
          ▼
    Store into serialParsedData[]

Only one block is active at a time.

------------------------------------------------------------------------

# Configuration File

Each line describes one parsing block.

    start_string;
    end_string;
    address;
    offset;
    offset;
    address;
    offset;
    offset;
    ...

## Rules

-   First token = block start marker.
-   Second token = block end marker.
-   Any non-integer token begins a new address.
-   Following integers belong to that address.
-   Offsets are interpreted in pairs.

Example:

``` text
Characteristic #2;
0x3ffbb61c;
0x3ffbb5fc;
4;5;6;7;8;9;10;11;2;3;
0x3ffbb60c;
0;1;
0x3ffbb5dc;
6;7
```

Equivalent structure:

``` text
Block
├── Start = Characteristic #2
├── End   = 0x3ffbb61c
├── Address 0x3ffbb5fc
│     ├── (4,5)
│     ├── (6,7)
│     ├── (8,9)
│     ├── (10,11)
│     └── (2,3)
├── Address 0x3ffbb60c
│     └── (0,1)
└── Address 0x3ffbb5dc
      └── (6,7)
```

------------------------------------------------------------------------

# Block Processing

When idle (`activeBlock == -1`), every incoming line is compared against
each configured start string.

When a start string matches:

-   that block becomes active
-   subsequent lines are parsed
-   parsing continues until the configured end string appears

Only one active block is allowed.

------------------------------------------------------------------------

# Address Matching

Within an active block, each line is searched for every configured
address string.

If an address is not found, that line is skipped for that address.

If found, every configured offset pair is evaluated.

------------------------------------------------------------------------

# Offset Interpretation

## Token Mode

Offsets count whitespace-separated fields.

    ADDR 10 AA BB CC DD

    offset 0 = 10
    offset 1 = AA
    offset 2 = BB

## Character Mode (`PS_CHAR_OFFSET`)

Offsets count characters after the address string.

    ADDR123456

    offset 0 = '1'
    offset 1 = '2'

------------------------------------------------------------------------

# Value Interpretation

## Hex Mode (default)

Each location must contain two hexadecimal digits.

    3A -> 0x3A

## ASCII Mode (`PS_ASCII_VALUE`)

The literal character becomes the byte.

    A -> 0x41

------------------------------------------------------------------------

# Endianness

Little-endian (default):

    34 12 -> 0x1234

Big-endian (`PS_BIG_ENDIAN`):

    12 34 -> 0x1234

If both offsets are identical, only one byte is read.

------------------------------------------------------------------------

# Packet Layout

The output packet is generated automatically.

The parser computes the destination index using `calculateOffsetIndex()`
by summing all offset counts in preceding blocks and addresses.

This means packet layout always matches configuration order. No output
indices appear in the configuration file.

------------------------------------------------------------------------

# Parser Mode Flags

  Flag               Meaning
  ------------------ ------------------------------
  `PS_CHAR_OFFSET`   Character offsets
  `PS_ASCII_VALUE`   ASCII instead of hexadecimal
  `PS_BIG_ENDIAN`    Big-endian byte ordering

Flags may be OR'ed together.

------------------------------------------------------------------------

# Example

Configuration

``` text
Characteristic #9;
I (28318992);
0x3ffbb64c;
0;
1
```

Input

``` text
Characteristic #9
0x3ffbb64c 34 12 FF AA
I (28318992)
```

Result

1.  Block activates.
2.  Address matches.
3.  Offsets `(0,1)` select first two bytes.
4.  Bytes become `0x1234`.
5.  Value stored in `serialParsedData[]`.
6.  End marker closes block.

------------------------------------------------------------------------

# Adding a New Protocol

1.  Identify start and end markers.
2.  Identify address strings.
3.  Determine byte positions.
4.  Choose parser mode flags.
5.  Add a configuration line.
6.  No parser code changes should be required.

------------------------------------------------------------------------

# Troubleshooting

  Symptom                 Possible Cause
  ----------------------- ------------------------------
  Block never activates   Incorrect start string
  Block never exits       Incorrect end string
  No values parsed        Address string mismatch
  Wrong values            Incorrect offsets
  Byte-swapped values     Wrong endianness
  Random values           Wrong ASCII/hex mode
  Missing values          Offset beyond available data

------------------------------------------------------------------------

Generated from the current parser implementation.
