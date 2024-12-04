let currentSortColumn = -1;
let ascending = true;

function isNumeric(n) {
    return !isNaN(parseFloat(n)) && isFinite(n);
}

function getValueFromString(inputString) {
    // Create a temporary div element
    var tempDiv = document.createElement('div');

    // Set the innerHTML of the div to the inputString
    tempDiv.innerHTML = inputString;

    // Access the input element by its name attribute
    var inputElement = tempDiv.querySelector('input[name="xxxx"]');

    // Check if the input element exists
    if (inputElement) {
        // Retrieve the value attribute
        var value = inputElement.value;

        // Log or use the value as needed
        return value;
    } else {
        return inputString;
    }
}

function sortTable(event, column) {
    startWaiting("Sorting...");

    // Ensure we have a valid event target and find the closest table
    const table = event.target.closest('.list');
    if (!table) {
        console.error("No table found.");
        stopWaiting();
        return;
    }

    // Select only the rows from the identified table
    const rows = table.querySelectorAll('.listrow');
    const sortingArray = Array.from(rows);
    sortingArray.sort((a, b) => {
        if (a.children[column] && b.children[column]) {
            let originalAValue = a.children[column].innerHTML.trim();
            let originalBValue = b.children[column].innerHTML.trim();
            if(a.children[column].hasAttribute("value")) { //if the span has a value, then use that, since otherwise we would have to parse out the value and know what units it was in.
                
                originalAValue = a.children[column].getAttribute("value");
                originalBValue = b.children[column].getAttribute("value");

            }
            let aValue = a.children[column].textContent.trim().replace(/<[^>]+>/ig, "");
            let bValue = b.children[column].textContent.trim().replace(/<[^>]+>/ig, "");
            if(a.children[column].hasAttribute("value")) {
                aValue = a.children[column].getAttribute("value");
                bValue = b.children[column].getAttribute("value");
            }
            if (originalAValue.toUpperCase().indexOf("<INPUT") == 0 && originalBValue.toUpperCase().indexOf("<INPUT") == 0) {
                aValue = getValueFromString(originalAValue);
                bValue = getValueFromString(originalBValue);
            } else {
                if (isNumeric(aValue) && isNumeric(bValue)) {
                    aValue = parseFloat(aValue);
                    bValue = parseFloat(bValue);
                }
            }

            if (aValue < bValue) return ascending ? -1 : 1;
            if (aValue > bValue) return ascending ? 1 : -1;

            return 0;
        }
    });

    if (rows.length > 0) {
        const parent = rows[0].parentNode;
        sortingArray.forEach(row => parent.appendChild(row));

        if (column === currentSortColumn) {
            ascending = !ascending;
        } else {
            currentSortColumn = column;
            ascending = true;
        }
    }
    stopWaiting();
}

// Attach event listeners to table headers
document.querySelectorAll('th').forEach((header, index) => {
    header.addEventListener('click', (event) => sortTable(event, index));
});
