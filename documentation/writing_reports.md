# Report Writer's Guide

*A guide to creating reports using the JSON-driven reporting engine.*

------------------------------------------------------------------------

# Introduction

The reporting system is designed so that new reports can be created
without writing PHP code. A report consists primarily of three pieces:

1.  A SQL query
2.  A JSON form definition
3.  An output definition

The reporting engine combines these pieces at runtime to generate
interactive reports, graphs, maps, tables, and custom HTML output.

                 Report Definition
            ┌─────────────────────────┐
            │   SQL Query             │
            │   Form JSON             │
            │   Output Definition     │
            └────────────┬────────────┘
                         │
                         ▼
                 Reporting Engine
                         │
          ┌──────────────┼──────────────┐
          ▼              ▼              ▼
      Line Graph      HTML Report     Table

# Anatomy of a Report

Every report contains three major components.

## SQL Query

``` sql
SELECT
    temperature,
    humidity,
    recorded
FROM device_log
WHERE device_id=<device_id/>
AND recorded > DATE_SUB(NOW(), INTERVAL <days/> DAY)
ORDER BY recorded;
```

Replacement tokens such as `<device_id/>` and `<days/>` are filled in
from the parameter form before execution.

## Form Definition

``` json
{
  "form":[
    {
      "name":"device_id",
      "type":"select",
      "values":"SELECT device_id,name AS text FROM device ORDER BY name"
    },
    {
      "name":"days",
      "type":"select",
      "range":"1...30",
      "value":7
    }
  ]
}
```

Each object in the `form` array becomes one user input.

## Output Definition

Outputs determine how the SQL results are displayed. A report may
provide one or more outputs including:

-   Line graphs
-   Tables
-   Maps
-   HTML templates

# SQL Queries

Any valid SQL supported by the database may be used.

Every form control creates a replacement token with the same name.

Example:

    "name":"device_id"

creates the SQL token

    <device_id/>

# Form Properties

  Property   Description
  ---------- ------------------------------
  `name`     Parameter name and SQL token
  `label`    Display label
  `type`     Input control type
  `value`    Default value
  `values`   Static array or SQL query
  `range`    Numeric range generator

## Supported Types

-   text
-   number
-   select
-   checkbox
-   bool
-   hidden
-   read_only
-   file
-   password
-   json
-   many-to-many

## Range

    "range":"1...30"

or

    "range":"0...100...5"

# Line Graph Output

Example:

``` json
{
  "outputDefault":"Line Graph",
  "output":[
    {
      "name":"Line Graph",
      "labelColumn":"recorded",
      "plots":[
        {
          "column":"temperature",
          "label":"Temperature",
          "color":"red",
          "lineWeight":1
        }
      ]
    }
  ]
}
```

Common plot properties:

  Property     Description
  ------------ ------------------
  column       SQL column
  label        Legend text
  color        Plot color
  lineWeight   Line thickness
  shape        Point appearance

# HTML Templates

Templates use Handlebars syntax.

``` handlebars
<h1>Temperature Records</h1>

<ul>
{{#each _}}
<li>
Temperature: {{temperature}}<br>
Time: {{recorded}}
</li>
{{/each}}
</ul>
```

Each SQL column becomes available to the template by name.

# Best Practices

-   Return only the columns required.
-   Always use `ORDER BY` for time-series graphs.
-   Use meaningful labels.
-   Provide sensible default values.
-   Alias calculated SQL fields.

# Future Topics

-   Token replacement
-   Map output
-   Table output
-   Multiple output formats
-   Dashboard reports
-   Cookbook examples
