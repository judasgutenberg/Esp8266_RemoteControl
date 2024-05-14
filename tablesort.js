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

function sortTable(column) {
    startWaiting("Sorting...");
 
    const rows = document.querySelectorAll('.listrow');
    const sortingArray = Array.from(rows);
    sortingArray.sort((a, b) => {
        let originalAValue = a.children[column].innerHTML.trim();
        let originalBValue = b.children[column].innerHTML.trim();
        //console.log(originalAValue);
        let aValue = a.children[column].textContent.trim().replace(/<[^>]+>/ig, "");
        let bValue = b.children[column].textContent.trim().replace(/<[^>]+>/ig, "");
        if (originalAValue.toUpperCase().indexOf("<INPUT")  == 0 && originalBValue.toUpperCase().indexOf("<INPUT") == 0) {
          aValue = getValueFromString(originalAValue);
          //console.log(aValue);
          bValue = getValueFromString(originalBValue);
        } else {
          if(isNumeric(aValue) && isNumeric(bValue)) {
              aValue = parseFloat(aValue);
              bValue = parseFloat(bValue);
          } 
        }

        if (aValue < bValue) return ascending ? -1 : 1;
        if (aValue > bValue) return ascending ? 1 : -1;


        return 0;
    });
  if(rows.length>0) {
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
