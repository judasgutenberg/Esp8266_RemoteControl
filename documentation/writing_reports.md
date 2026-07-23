# Report Writer's Guide

*A guide to creating reports using the JSON-driven reporting engine.*

------------------------------------------------------------------------

# Introduction

The reporting system is designed so that new reports can be created
entirely within a web-based tool, though one where you have detailed control and vast capabilities. 
The idea here was to make it so you can concentrate on the specific data you want to retrieve (which is always most easily specified in actual SQL) 
without worrying too much about presentation details, though if you want to worry about that, you can.

A report consists primarily of three pieces:

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

# Working With the Report Tools

Navigate to /report on your server:

![alt text](report_list.jpg?raw=true)

Click edit to edit a report:

![alt text](report_editor.jpg?raw=true)

Or run the report, which presents a form if it accepts parameters:

![alt text](report_runner.jpg?raw=true)

Then examine your results in whatever format you requested:

![alt text](report_results.jpg?raw=true)

# Form Properties

  Property   Description
  ---------- ------------------------------
  `name`     Parameter name and SQL token
  `label`    Display label
  `type`     Input control type
  `value`    Default value
  `values`   Static array or SQL query, resulting in a dropdown where, in the select, `value` is the value and `text` is the human-readable label
  `range`    Numeric range generator

## Supported Types

-   text: any sort of string data
-   number: a numeric value
-   select: if a values parameter is included and it is a valid SQL expression, a dropdown pre-populated with results from the query is provided.
-   checkbox: a checkbox in the UI or true/false values
-   bool: a set of radio buttons in the UI for true/false values
-   hidden: no UI item support is supplied in the form, but a default value can be sent from the form
-   read_only: a visible element is provided in the form that cannot be changed
-   file:  a file can be uploaded to the file system with a reference to it placed in the database
-   password: an obfuscated form item
-   json:  a JSON object
-   many-to-many: a form item that allows several items from another table to be linked to our form item

## Range

    "range":"1...30": numbers 1 to 30 in increments of 1

or

    "range":"0...100...5": numbers 0 to 100 in increments of 5

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

# Report Log
Every report that is run is logged in the report_log table, complete with the values of any parameters used. Additional information about the time taken to run the report and the number of records returned is also collected.  The user interface provides a way to easily re-run the same query from the report_log, defaulting the form to the parameters used but also allowing them to be changed.  One report, "RunSQL," takes as its single parameter an arbitrary SQL expression, which could of course be any query you want to run, including queries to create, modify, or drop tables.  Once built, that report was mostly how I built the rest of the database schema. Obviously not just any user should be permitted to run that report!


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
