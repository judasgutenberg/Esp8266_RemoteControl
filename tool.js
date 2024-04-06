function autoUpdate(encryptedSql, headerData, tableId){
  var decodedHeaderData = JSON.parse(headerData);
  var xmlhttp = new XMLHttpRequest();
  xmlhttp.onreadystatechange = function() {
 
    if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
      //console.log(xmlhttp.responseText);
      var data = JSON.parse(xmlhttp.responseText);
      let tableRows = document.getElementsByClassName("listrow");
      
      rowCounter = 0;
      for (const row of tableRows) {
        var htmlRow = tableRows[rowCounter];
        var dataRecord = data[rowCounter];
        if(htmlRow) {
          //console.log(htmlRow);
          var spans = htmlRow.getElementsByTagName("span");
          cellCounter = 0;
          for (const column of decodedHeaderData){
            //console.log(column);
            let key = column["name"];
            let newcolumnData = dataRecord[key];
            //console.log(key, newcolumnData);
            if(spans[cellCounter].innerHTML.indexOf("<input") == -1) {
              spans[cellCounter].textContent = newcolumnData;
            }
            cellCounter++;
          }
        }
        rowCounter++;

      }
    }
    //gotta reapply latest sort:
    //nah, let's not do it this way
    /*
    if(currentSortColumn > -1) {
      ascending = -ascending;
      sortTable(currentSortColumn);
    }
    */
  }
  const params = new URLSearchParams();
  params.append("direction", ascending);
  params.append("sortColumn", currentSortColumn);
  params.append("action", "runencryptedsql");
  params.append("headerData",  headerData.replace(/'/g, "\\'"));
  params.append("value",  encryptedSql);

  let url = "tool.php"; 
  //console.log(url);
  xmlhttp.open("POST", url, true);
  xmlhttp.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
  xmlhttp.send(params);
  setTimeout(function() { 
    autoUpdate(encryptedSql, headerData, tableId);
  }, 7000);

}




 








function copy(id){
  var href = document.getElementById("href" + id.toString());
  var contentDiv = document.getElementById("clip" + id.toString());
  var clipContent = "";
  if(href) {
    clipContent = href.innerHTML;
  } else if(contentDiv) {
    clipContent = contentDiv.innerHTML;
  }
  if(clipContent) {
    copyHack(clipContent);
    var backgroundColor = document.body.style.backgroundColor;
    document.body.style.backgroundColor = '#ccffcc';
    setTimeout(function () {document.body.style.backgroundColor = backgroundColor}, 200);
  }
}



function copyHack(value){ //such a hack!!
    var copyTextarea = document.createElement("textarea");
    copyTextarea.style.position = "fixed";
    copyTextarea.style.opacity = "0";
    copyTextarea.textContent = value;
 
    document.body.appendChild(copyTextarea);
    copyTextarea.select();
    document.execCommand("copy");
    document.body.removeChild(copyTextarea);
}

function cleanText(inputText) {
  const tempDiv = document.createElement('div');
  tempDiv.innerHTML = inputText;
  const cleanedText = tempDiv.innerText.trim();
  return cleanedText;
}
 
function unhighlightAllIssues(){
  let issues = document.getElementsByClassName("issuelist");
  for(var issue of issues){
    issue.style.borderColor="#ffffff"; 
    issue.style.borderStyle="none"; 
  }
}

function highlightIssue(issueId) {
  unhighlightAllIssues();
  unhighlightAllRectangles();
  var issue = document.getElementById(issueId);
  if(issue) {
    issue.style.borderColor="#000000"; 
    issue.style.borderWidth="1px"; 
    issue.style.borderStyle="solid"; 
  } 
}

function unhighlightAllRectangles(){
  let rectangles = document.getElementsByClassName("issueHighlight");
  for(var rectangle of rectangles){
    //console.log(rectangle);
    rectangle.style.borderWidth=1;
    rectangle.style.opacity=0.3;
  }
}

function highlightRectangle(rectCount) {
  unhighlightAllIssues();
  unhighlightAllRectangles();
  var rect = document.getElementById("rect-" + rectCount);
  var rect2 = document.getElementById("rect2-" + rectCount);
  if(rect) {
    rect.style.borderWidth=4; 
    rect.style.opacity=0.1;
  }
  if(rect2) {
    rect2.style.borderWidth=4;
    rect2.style.opacity=0.1;
  }
  
}

function issueMouseOver(issueId) {
  highlightIssue(issueId);
}


function hideIssuePopup() {
  let bubble = document.getElementById('issuePopup');
  if (bubble) {
      bubble.style.display = 'none';
  }
}


function getFormValues(formId) {
  var form = document.getElementById(formId);
  var formData = {};

  if (form) {
      var elements = form.elements;

      for (var i = 0; i < elements.length; i++) {
          var element = elements[i];
          var elementType = element.type;
          var elementName = element.name;

          if (elementName) {
              switch (elementType) {
                  case 'select-one':
                      formData[elementName] = element.options[element.selectedIndex].value;
                      break;
                  case 'select-multiple':
                      formData[elementName] = getSelectedOptions(element);
                      break;
                  case 'checkbox':
                  case 'radio':
                      formData[elementName] = element.checked ? element.value : null;
                      break;
                  default:
                      formData[elementName] = element.value;
              }
          }
      }
  }

  return formData;
}

function getSelectedOptions(selectElement) {
  var selectedOptions = [];
  for (var i = 0; i < selectElement.options.length; i++) {
      if (selectElement.options[i].selected) {
          selectedOptions.push(selectElement.options[i].value);
      }
  }
  return selectedOptions;
}

document.addEventListener('mousemove', function(event) {
  // Update last known mouse position
  lastMouseX = event.clientX;
  lastMouseY = event.clientY;
});

function dictionaryTool(issueId, word){ //displays the HTML form from the word editor in an overlay div and reads it via javascript so we have a nice modal-style popup word editor.
  var xmlhttp = new XMLHttpRequest();
  var outerXmlhttp = new XMLHttpRequest();
  var formSaveXmlhttp = new XMLHttpRequest();
  var wordId = null;
  var wordListId = null;
  var type = null;
  outerXmlhttp.onreadystatechange = function() {
 
    if (outerXmlhttp.readyState == 4 && outerXmlhttp.status == 200) {
      
      var data = JSON.parse(outerXmlhttp.responseText);
      var datum = null;
      if (data && data.length > 0) {
        datum = data[0];
        wordId = datum["word_id"];
        wordListId = datum["word_list_id"];
        type = datum["type"];
      }
      xmlhttp.onreadystatechange = function() {

        if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
          //alert(xmlhttp.responseText);
          var html = xmlhttp.responseText;
          var toolName = "dictionaryTool";
          let tool = document.getElementById(toolName);
          if (!tool) {
            tool = document.createElement('div');
            tool.id = toolName;
            tool.className = toolName;
          }
          //put div near where the mouse is
          tool.style.left = (lastMouseX - tool.clientWidth / 2) + 'px';
          tool.style.top = (lastMouseY - tool.clientHeight / 2)

          tool.innerHTML = "";

          closex = document.createElement('div');
          closex.innerHTML = "<div onclick='document.getElementById(\"" + toolName + "\").style.display=\"none\"'>x</div>";
          closex.className = 'closex';
          tool.appendChild(closex);

          document.body.appendChild(tool);

          tool.style.display = 'block';
          tool.innerHTML += html;

          var submit = document.getElementById('action');
          submit.onclick = function() { 
            //get by input tagname
            var data = getFormValues("genericform");
            const params = new URLSearchParams();
            for (const key in data) {
                if (data.hasOwnProperty(key)) {
                    params.append(key, data[key]);
                }
            }
            let url = "."; 
            formSaveXmlhttp.onreadystatechange = function() {

              if (formSaveXmlhttp.readyState == 4 && formSaveXmlhttp.status == 200) {
                var html = formSaveXmlhttp.responseText;
              }
            }
        
            formSaveXmlhttp.open("POST", url, true);
            formSaveXmlhttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
            formSaveXmlhttp.send(params.toString());
    
            tool.style.display = 'none';
            return false;
          };
        }
      }
      //url for saving a word:
      //let url = "?action=save+word&datatype=json&table=word&word=" + encodeURIComponent(wordText) + "&word_id=" +  wordId + "&word_list_id=" + wordListId + "&type=" +  encodeURIComponent(type); 
      let url = "?action=getform&datatype=html&table=word&word=" + encodeURIComponent(word); 
      if(wordId) {
        url += "&word_id=" + wordId;

      }
      if(wordListId) {
        url += "&word_list_id=" + wordListId;

      }
      if(type) {
        url += "&type=" + type;

      }
      console.log(url);
      xmlhttp.open("GET", url, true);
      xmlhttp.send();
    }
  }

  let url = "?action=search&datatype=json&table=word&search_term=" + encodeURIComponent(word); 
  console.log(url);
  outerXmlhttp.open("GET", url, true);
  outerXmlhttp.send();
}

// issuePopup(event, text, thisPage, rect)
function issuePopup(text, thisPage, issueId, documentId, rect, xDelta){
  console.log("actual popup");
  hideIssuePopup();

  if(!xDelta){ //to position the window leftward in some cases
    xDelta = 0;
  }
  var xmlhttp = new XMLHttpRequest();
 
  xmlhttp.onreadystatechange = function() {
 
    if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
      //alert(xmlhttp.responseText);
      var data = JSON.parse(xmlhttp.responseText);
      var toolName = 'issuePopup';
      let bubble = document.getElementById(toolName);
            
      // If the bubble doesn't exist, create it
      if (!bubble) {
          bubble = document.createElement('div');
          bubble.id = toolName;
          bubble.className = toolName;
          document.body.appendChild(bubble);

      }
      
      bubble.innerHTML = "";
      closex = document.createElement('div');
      closex.innerHTML = "<div onclick='document.getElementById(\"" + toolName + "\").style.display=\"none\"'>x</div>";
      closex.className = 'closex';
      bubble.appendChild(closex);

 
      // Set the content and position of the bubble
 
      
      var otherPageLinks = "";
      bubble.innerHTML += "<p/><div><b><i>" + text + "</i></b></div><p/>";
      if(data){
        for(var i=0; i<data.length; i++){
          var datum = data[i];
          //console.log(thisPage, datum["page_number"]);
          if(thisPage != datum["page_number"]){
            otherPageLinks += "<div><a href='?table=document&document_id=" + datum["document_id"] + "&action=browse&page=" + datum["page_number"]  + "'>Page " + datum["page_number"]  + "</a></div>";
          }
        }
        
        if(otherPageLinks != ""){
          bubble.innerHTML += "<div><b>Other pages with this issue:</b></div>";
          bubble.innerHTML += otherPageLinks;
        } else {
          bubble.innerHTML += "<div><b><i>No other pages with this issue</i></b></div>";

        }
        if(datum["note"] == null || datum["note"] == "null"  || datum["note"] == "" || typeof(datum["note"]) == "undefined") {
          
          datum["note"]  = "";
        }
        bubble.innerHTML += "<div>Note: <form><input type='hidden' id='issue_id' name='issue_id' value='" + issueId + "'/><textarea onkeyup='noteUpdate()' id='note' name='note' style='width:400px;height:50px'/>" + datum["note"] + "</textarea></form></div><p/>";
        bubble.innerHTML += "<div class='issuetoollink'><a href=\"javascript:setCriticality(" + datum["document_id"] + "," + datum["scan_id"] +"," + issueId + ",'',500)\">Turn off this issue.</a></div><p/>";
        bubble.innerHTML += "<div class='issuetoollink'><a href=\"javascript:setCriticality(" + datum["document_id"] + "," + datum["scan_id"] +",'','" + text.replace(/'/g, "\\'") + "',500," + thisPage + ")\">Turn off this issue for this page.</a></div><p/>";
        bubble.innerHTML += "<div class='issuetoollink'><a href=\"javascript:setCriticality(" + datum["document_id"] + "," + datum["scan_id"] +",'','" + text.replace(/'/g, "\\'") + "',500)\">Turn off this issue for this document</a>.</div><p/>";
        bubble.innerHTML += "<div class='issuetoollink'><a href=\"javascript:dictionaryTool(" + issueId + ",'" + text +"')\">Add/edit word to/in dictionary.</a></div><p/>";
        //bubble.innerHTML += "<div class='issuetoollink'>Turn off this issue for all documents.</div><p/>";//we'll save this idea for Version 2.0
      }  else {

        bubble.innerHTML += "<div>This document has not been scanned.</div>"
      }
    
      //bubble.innerHTML += "<div><a href='javascript:hideIssuePopup()'>Close</a></div><p/>";
      bubble.style.left = (parseInt(lastMouseX) + parseInt(xDelta)) + 'px';
      bubble.style.top = lastMouseY + 'px';
    
      // Show the bubble
      bubble.style.display = 'block';
 
      bubble.addEventListener('mouseup', function(event) {
     
        //hideIssuePopup(); 
      });
      
    }
  };
  let url = "?datatype=json&table=scan&document_id=" + documentId + "&scan_id=latest&issue_content=" +  encodeURIComponent(text); 
  console.log(url);
  xmlhttp.open("GET", url, true);
  xmlhttp.send();
 
  
}

function setCriticality(documentId, scanId, issueId, issueValue, criticality, page) {
  var xmlhttp = new XMLHttpRequest();
 
  xmlhttp.onreadystatechange = function() {
 
    if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
      //alert(xmlhttp.responseText);
      hideIssuePopup();
      window.location.reload();
    }
  }
  let url = "?action=setcriticality&table=issue&issue_content=" + encodeURIComponent(issueValue) + "&issue_id=" + issueId + "&scan_id=" + scanId + "&document_id=" +  documentId + "&criticality=" + criticality + "&page=" + page; 
  console.log(url);
  xmlhttp.open("GET", url, true);
  xmlhttp.send();
  
}

function noteUpdate(){
  let noteTextbox = document.getElementById('note');
  let issueId = document.getElementById('issue_id').value;
  let note = noteTextbox.value;
  var xmlhttp = new XMLHttpRequest();
 
  xmlhttp.onreadystatechange = function() {
 
    if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
      //alert(xmlhttp.responseText);
    }
  }
  let url = "?action=savenote&table=issue&issue_id=" + issueId + "&note=" +  encodeURIComponent(note); 
  console.log(url);
  xmlhttp.open("GET", url, true);
  xmlhttp.send();

}

function getSpacePositions(str) {
  return str.split('').reduce(function(acc, char, index) {
    if (char === ' ') {
      acc.push(index);
    }
    return acc;
  }, []);
}


function drawRectangle(startOffset, length, thisTest, thisPage, issueId, documentId, text, rectangleCount) {
  //console.log(text);
  if(!text){
    return;
  }
  //console.log(text.length);
  const textDiv = document.getElementById('textblock');
 
  var xFudge = -351;
  var yFudge = -146;
  var xFactor = 1.0;
  var yFactor = 1.0;
  xFudge = -textDiv.offsetLeft -2;
  yFudge = -textDiv.offsetTop -2;
  width = textDiv.offsetWidth;
  height = textDiv.offsetHeight;
  //console.log(xFudge, yFudge, width, height,  window.scrollX,  window.scrollY);
  // Create a new div for the rectangle
  const rectangleDiv = document.createElement('div');
  const rectangleDiv2 = document.createElement('div');
  rectangleDiv.classList.add('issueHighlight');
  rectangleDiv2.classList.add('issueHighlight');
  rectangleDiv.id = "rect-" + rectangleCount;
  rectangleDiv.id = "rect2-" + rectangleCount;
  // Set the dimensions and position of the rectangle
  const range = document.createRange();
  const textNode = textDiv.firstChild;
  //console.log(textDiv.offsetLeft, textDiv.offsetTop);
  //console.log(textNode, textNode.length);
 
 
  //const span = document.createElement('span');
  startOffset = parseInt(startOffset);
  length = parseInt(length);
  //console.log(textNode.length);
 
  range.setStart(textNode, startOffset + 1);
  range.setEnd(textNode, startOffset + length + 1);
  //range.surroundContents(span);
  //var rects = getLineRectangles(range);
  
  let rect = range.getBoundingClientRect();
  let range2 = null;
  let rect2 = null;
 
 

    //console.log(rect);
    //console.log(thisTest.markup_color);
 
    var tooTallThreshold = 15;
    //if a issue text spans two lines, even sometimes from an adjoining space, getBoundingClientRect() returns a rectangle that spans all of two lines for the entire width of the text column.
    //so i look for a space that breaks the text into two separate lines and draw separate rectangles for both. the onclick events for both refer to the same issue
    //there's probably a better way to do this, but none of the suggestions from chatGPT worked, so i came up with this method myself
    if(rect.height > tooTallThreshold  && length< 45){
      //console.log(text);
      //console.log(text.indexOf("/n"));
      //console.log(rect.height);
      //rect.width= 30;
      //rect.height = 13;
      //top = top + 12;
      //temporarily disabled all these:
      var spacePositions = getSpacePositions(text);
      var goodEnd = 0;
      //console.log(spacePositions);
      var spacePositionTrickWorked = false;
      for(var spacePosition of spacePositions){
        //console.log(spacePosition);
        var newLength = length - spacePosition;
        range.setStart(textNode, parseInt(startOffset) + 1);
        range.setEnd(textNode, (parseInt(startOffset) + 1 + parseInt(newLength)) -  (parseInt(newLength) - parseInt(spacePosition)));
        //if we're going for later part:
        //range.setStart(textNode, parseInt(startOffset) + parseInt(spacePosition) + 1);
        //range.setEnd(textNode, parseInt(startOffset) + parseInt(spacePosition) + 1 + parseInt(newLength));
        rect = range.getBoundingClientRect();
        console.log(text, rect.height, rect.width);
        if (rect.height > tooTallThreshold){
          //break;
          range.setStart(textNode, parseInt(startOffset) + 1);
          range.setEnd(textNode, (parseInt(startOffset) + 1 + parseInt(newLength)) -  (parseInt(newLength) - parseInt(goodEnd)));
          rect = range.getBoundingClientRect();
          range2 = document.createRange();
          range2.setStart(textNode, parseInt(startOffset) + 2 + parseInt(goodEnd));
          range2.setEnd(textNode, parseInt(startOffset) + parseInt(length) + 1);
          rect2 = range2.getBoundingClientRect();
          //console.log(parseInt(startOffset) + 1 + parseInt(goodEnd), parseInt(startOffset) + parseInt(length) + 1);
          //console.log(text, rect2.height, rect2.width);
          spacePositionTrickWorked = true;
          break;
        } else {
          goodEnd = spacePosition;
        }

      }
      if(!spacePositionTrickWorked){
        console.log("ok do this then; hopefully don't have to");
        //range.setStart(textNode, startOffset + 1);
        //range.setEnd(textNode, startOffset + length-2);
        //rect = range.getBoundingClientRect();
      }
      //var newLength = length - spacePosition;
      //console.log(startOffset, length, spacePosition, newLength, text);
      //range.setStart(textNode, startOffset + spacePosition + 1)
      //range.setEnd(textNode, startOffset + spacePosition + 1 + newLength);
      //range.setStart(textNode, startOffset + 2);
      //range.setEnd(textNode, startOffset + 2 + length);
  
    }
    var top = rect.top;
    rectangleDiv.style.width = 1.0 * (rect.width) + 'px';
    rectangleDiv.style.height = rect.height + 'px';
    rectangleDiv.style.top = yFudge + top + window.scrollY + 'px';
    rectangleDiv.style.left = xFudge +  (rect.left) + window.scrollX + 'px';
    rectangleDiv.style.backgroundColor = thisTest.markup_color;
    rectangleDiv.style.color = '#000000';
    rectangleDiv.style.cursor = 'pointer';
    if(rect2){
      var top = rect2.top;
      rectangleDiv2.style.width = 1.0 * (rect2.width) + 'px';
      rectangleDiv2.style.height = rect2.height + 'px';
      rectangleDiv2.style.top = yFudge + top + window.scrollY + 'px';
      rectangleDiv2.style.left = xFudge +  (rect2.left) + window.scrollX + 'px';
      rectangleDiv2.style.backgroundColor =  thisTest.markup_color;
      rectangleDiv2.style.color = '#000000';
      rectangleDiv2.style.cursor = 'pointer';
      rectangleDiv2.addEventListener('click', function(event) {
        issuePopup(text, thisPage, issueId, documentId, rect2);
  
      });
      textDiv.appendChild(rectangleDiv2);

    }
    rectangleDiv.addEventListener('mouseover', function() {
      issueMouseOver("issue" + rectangleCount);
    });
    rectangleDiv.addEventListener('click', function(event) {
      issuePopup(text, thisPage, issueId, documentId, rect);
    });

    textDiv.appendChild(rectangleDiv);
 
 
}



function startWaiting(message){
  var waitingBody = document.getElementById("waitingouter");
  waitingBody.style.display = 'block';
  var waitingMessage = document.getElementById("waitingmessage");
  waitingMessage.innerHTML = message;
}
function stopWaiting(){
  var waitingBody = document.getElementById("waitingouter");
  waitingBody.style.display = 'none';
  
}

function deleteListRows(idOfParent, classToKill) { //thanks chatgpt!
  var parentDiv = document.getElementById(idOfParent); 
  var listRows = parentDiv.getElementsByClassName(classToKill);
  while (listRows.length > 0) {
      parentDiv.removeChild(listRows[0]);
  }
}
 

function searchWords(word, wordListId){
  //text = "I often envision a giant protective bubble over our property and inside it a place where everything is right in the world, the way we want it to be. Animals roam free, living happy and peaceful lives the way they should. They are free to be themselves, among friends and , in some cases, family. There is no more fea r of harm , no want for food or water, warmth or shelter. They have everything they need. They are loved and treate d with respect and compassion. We co exist with them, never considering ourselves superior or their “owners. ” We don ’t use them as commodities or exploit them in any way. They owe us nothing. But what they do give, unconsciously, is the greatest asset to o ur work. They are ambassadors for all others like them, showing humans that other animals are not mere automatons. Visitors to Woodstock Farm Animal Sanctuary also bear witness to the scars left by industrial confinement, cruel handling and procedures, and the long -term effects of breeding for productivity. They can see and touch the battery cages, farrowing crate (for pregnant pigs) , and veal crate that we have on display for educational purposes. Visitors also bear witness to the love and care we provide to these animals as our staff caretakers and volunteers perform physical therapy, administer medications , and 123 Quoted in Joshua Cohen, The Arc of the Moral U niverse and Other Essay s (Cambridge, MA: Harvard University Press , 2010 ) 17.";
  var list = document.getElementById("list");
  var xmlhttp = new XMLHttpRequest();
  var out = "";
  deleteListRows('list', 'listrow');
  var toolTemplate = "<a href='?table=word&word_id=<word_id/>'>view</a>"
  xmlhttp.onreadystatechange = function() {
    if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
      //alert(xmlhttp.responseText);
      var data = JSON.parse(xmlhttp.responseText);
      //console.log(data);
      
      for(var i=0; i<data.length; i++){
        var datum = data[i];
        //console.log(datum);
        out += "<div class='listrow'>";
        out += "<span>" + datum["word"] + "</span>";
        out += "<span>" + datum["type"] + "</span>";
        out += "<span>" + datum["name"] + "</span>";
        
        out += "<span>" + toolTemplate.replace("<word_id/>", datum["word_id"]) + "</span>";
        out += "</div>";
      }
      list.innerHTML += out;
    }
  };
  let url = "?datatype=json&table=word&action=browse&word_list_id=" + wordListId + "&search_term=" + encodeURIComponent(word); 
  xmlhttp.open("GET", url, true);
  xmlhttp.send();
}

if(document.getElementById('file')) {
  document.getElementById('file').onchange = function() {
    startWaiting();
  };
}
window.addEventListener('resize', function() {
  window.location.reload();
});

function genericListActionBackend(name, value, tableName, primaryKeyName, primaryKeyValue){ //this is a security nightmare;  need to improve!!
  var xmlhttp = new XMLHttpRequest();
  xmlhttp.onreadystatechange = function() {
    if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
      //alert(xmlhttp.responseText);
      //var data = JSON.parse(xmlhttp.responseText);
 
    }
  };
  let url = "?action=genericFormSave&table_name=" + encodeURIComponent(tableName) + "&primary_key_name=" + encodeURIComponent(primaryKeyName) + "&primary_key_value=" + encodeURIComponent(primaryKeyValue) + "&value=" + encodeURIComponent(value) + "&name=" + encodeURIComponent(name); 
  xmlhttp.open("GET", url, true);
  xmlhttp.send();
}
