
let glblChart = null;
//For graphs info, visit: https://www.chartjs.org
let graphDataObject = {}
let multiGraphDataObject = [];
let pastYearsViewed = [];
let yearsIntoThePastWeCareAbout = [0,1,2,3];
let timeStamp = [];
//let devices = [];


resetGraphData();

function resetGraphData(deviceIdArray){
	multiGraphDataObject = {};
	graphDataObject = {};
	pastYearsViewed = [];
	for (let year of yearsIntoThePastWeCareAbout) { 
		if(deviceIdArray){
			if (!multiGraphDataObject[year]) {
				multiGraphDataObject[year] = {};
			}
			for(let specificdeviceId of deviceIdArray){
				multiGraphDataObject[year][specificdeviceId] = {"values": [], "timeStamps": []};
			}
		}
		for (let column of columnsWeCareAbout) {
			if (!graphDataObject[year]) {
				graphDataObject[year] = {};
			}

			if (!graphDataObject[year][column]) {
				graphDataObject[year][column] = [];
			}
		}
	}
}

function addPastYearToGraph(deviceIdArray, deviceId, yearsAgo, plotType){
	yearsAgo = parseInt(yearsAgo);
	let columnCount = 0;
	let colorSeries = ["#ffcccc","#ccffcc", "#ccccff", "#999999", "#aaaaaa", "#bbbbbb", "#cccccc"];
	if(plotType == "multi"){
		for(let deviceId of deviceIdArray){
			let yAxisId = "A";
			let foundDevice = findObjectByColumn(devices, "device_id", deviceId);
			let label;
			let color = colorSeries[columnCount];
			if(foundDevice){
				label = foundDevice["location_name"];
				if(foundDevice["color"]){
					color = tinycolor(foundDevice["color"]).lighten(26);
				}
			}
			glblChart.data.datasets.push(
					{
						label: label + " " + parseInt(parseInt(new Date().getFullYear()) - yearsAgo),
						fill: false,  //Try with true
						backgroundColor: color,
						borderColor: color,
						data: multiGraphDataObject[yearsAgo][deviceId].values,
						yAxisID: yAxisId,
						tension: 0.1
					}
			);	
			columnCount++;	
			//console.log(multiGraphDataObject[yearsAgo][deviceId]);
		}

	} else {  //not multi
		columnCount = 0;
		//console.log(graphDataObject);
		if(graphDataObject[yearsAgo]){
			for (let column of columnsWeCareAbout){
				let mapToUse;
				let foundDevice = findObjectByColumn(devices, "device_id", deviceId.toString());
				let deviceColumnMaps = foundDevice["device_column_maps"];
				for(let map of deviceColumnMaps){
					if(column == map["column_name"] && map["table_name"] == "device_log"){
						mapToUse = map;
					}
				}
				if(gvfa("include_in_graph", mapToUse) == 1 || gvfa("include_in_graph", mapToUse) == null) {
					let color = colorSeries[columnCount];
					let label = column;
					color = gvfa("color", mapToUse, color);
					color = tinycolor(color).lighten(26);
					label = gvfa("column_name", mapToUse, label);
					label = label + " " + parseInt(parseInt(new Date().getFullYear()) - yearsAgo);
					//console.log(graphDataObject);
					let yAxisId = "A";
						if(column == "pressure"){
							yAxisId = "B";
						}
					glblChart.data.datasets.push(
							{
								label: label,
								fill: false,  //Try with true
								backgroundColor: color,
								borderColor: color,
								data: graphDataObject[yearsAgo][column],
								yAxisID: yAxisId
							}
					);
					columnCount++;
				}
			}
		}
	}
	//console.log(glblChart.data.datasets);
	glblChart.update();
}

function showGraph(deviceId, plotType){
	let colorSeries = ["#ff0000", "#00ff00", "#0000ff", "#009999", "#3300ff", "#ff0033", "#ff3300", "33ff00", "#0033ff", "#6600cc", "#ff0066", "#cc6600", "66cc00", "#0066cc"];
	if(glblChart){
		glblChart.destroy();
	}
    let ctx = document.getElementById("Chart").getContext('2d');
	let timeStampLabels = [];

	//graphDataObject[yearsAgo][column]
	let chartDataSet = [];
	let columnCount = 0;
	let foundDevice = findObjectByColumn(devices, "device_id", deviceId.toString());
	let deviceColumnMaps = foundDevice["device_column_maps"];
	if(plotType == "single"){
		for (let column of columnsWeCareAbout){
			let mapToUse;
			for(let map of deviceColumnMaps){
				if(column == map["column_name"] && map["table_name"] == "device_log"){
					mapToUse = map;
				}
			}
			if(gvfa("include_in_graph", mapToUse) == 1 || gvfa("include_in_graph", mapToUse) == null) {
				let graphSubtitle;
				let color = colorSeries[columnCount];
				let label = column;
				color = gvfa("color", mapToUse, color);
				label = gvfa("display_name", mapToUse, label);
				let yAxisId = "A";
				if(column == "pressure"){
					yAxisId = "B";
				}
				graphSubtitle =   foundDevice["location_name"] + " data";
	
				//console.log(graphDataObject[0][column]);
				chartDataSet.push(
					{
						label: label,
						fill: false,  //Try with true
						backgroundColor: color,
						borderColor: color,
						data: graphDataObject[0][column],
						yAxisID: yAxisId
					}
				);
				columnCount++;
			}
		}
	}
	let scales = {
				x: {
          type: 'time',
          title: {
            display: true,
            text: 'Time'
          }
        },
        
        A: {
          type: 'linear',
          position: 'left',
          title: {
          display: true,
          text: 'temperature/humidity'
          }
        },
        B: {
          type: 'linear',
          position: 'right',
          title: {
          display: true,
          text: 'pressure'
				},
				grid: {
          drawOnChartArea: false
				}
      }
   }
	timeStampLabels = timeStamp;
	let graphSubtitle = "Weather Data";
	//console.log(devices);
	if(devices && devices.length>0){
 		graphSubtitle =   findObjectByColumn(devices, "device_id", deviceId + '')["location_name"] + " data";
	}
	if(plotType == "multi"){
		timeStampLabels = [];
		chartDataSet = [];
		//console.log("what gets graphed", multiGraphDataObject);
		let colorCursor = 0;
		let thisAxis = 'A';
		for(let key in multiGraphDataObject[0]){
			let value = multiGraphDataObject[0][key];
			//console.log("key:", key);
			//console.log(value["values"]);
			let foundDevice = findObjectByColumn(devices, "device_id", key);
			let label;
			let color = colorSeries[columnCount];
			if(foundDevice){
				label = foundDevice["location_name"];
				if(foundDevice["color"]){
					color = foundDevice["color"];
				}
			}
			if(label){
				chartDataSet.push(
					{
					label: label,
					fill: false,  //Try with true
					backgroundColor: color, //Dot marker color
					borderColor: color, //Graph Line Color
					data: value["values"],
					tension: 0.1,
					yAxisID: thisAxis
					}
				);
			}
			//console.log(value.timeStamps);
			//if(key == 1) {
				//timeStampLabels = [];
				timeStampLabels.push(...value.timeStamps);
			//}
			colorCursor++;
		}
		scales =  {
				x: {
          type: 'time',
          title: {
            display: true,
            text: 'Time'
          }
        },
        A: {
          type: 'linear',
          position: 'left',
          title: {
          display: true,
          text: ''
          }
       }
    }
		//console.log(timeStampLabels);
		graphSubtitle = document.getElementById("specific_column")[document.getElementById("specific_column").selectedIndex].value + " data";

	}
	timeStampLabels.sort();
	//console.log(timeStampLabels);
    let weatherChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: timeStampLabels,  //Bottom Labeling
            datasets: chartDataSet,
        },
        options: {
			responsive: true,
            hover: {mode: null},
            title: {
                    display: true,
                    text: graphSubtitle
                },
            maintainAspectRatio: false,
            elements: {
            line: {
                    tension: 0.5 //Smoothening (Curved) of data lines
                }
            },
            scales: scales,
			spanGaps: true  // Connects the dots, even if there are gaps (null values)
        }
    });
	return weatherChart;
}

//On Page load show graphs
window.onload = function() {
  //console.log(new Date().toLocaleTimeString());
  let deviceId = document.getElementById("locationDropdown")[document.getElementById("locationDropdown").selectedIndex].value;
  officialWeather(deviceId);
  //showGraph(5,10,4,58);W
};
 
let currentStartDate; //a global that needs to persist through HTTP sessions in the frontend
let justLoaded = true;

function getWeatherData(yearsAgo) {
	const queryParams = new URLSearchParams(window.location.search);
	let deviceIdArray = [];
	let scale = queryParams.get('scale');
	let deviceId = queryParams.get('location_id');
	let periodAgo = queryParams.get('period_ago');
	let deviceIds = queryParams.get('location_ids');
	let plotType = "single";
	let specificColumn = queryParams.get('specific_column');
	let absoluteTimespanCusps = queryParams.get('absolute_timespan_cusps');
	let atcCheckbox = document.getElementById("atc_id");
	let yearsAgoToShow = queryParams.get('years_ago');
	let greatestTime = "2000-01-01 00:00:00";
  let absoluteTimeAgo = queryParams.get('absolute_time_ago');
  
	let url = new URL(window.location.href);

	if(yearsAgo > 0 && pastYearsViewed.indexOf(yearsAgo) < 0){
		pastYearsViewed.push(yearsAgo);
	} else if (yearsAgo > 0) {
		return; //prevent responding to the the yearsAgo button more than once for a year
	}

	if(!yearsAgo){
		yearsAgo = 0;
	}
	if(yearsAgoToShow){
		url.searchParams.set("years_ago", yearsAgoToShow);
	}
	for(let radio of document.getElementsByName("plottype")){
		if(radio.checked){
			plotType = radio.value;
		}
	}
	
	url.searchParams.set("plot_type", plotType);

	if(atcCheckbox.checked) {
		absoluteTimespanCusps = 1;
	} else {
		absoluteTimespanCusps = 0;
	}
	if(justLoaded){
		if(absoluteTimespanCusps == 1){
			atcCheckbox.checked = true;
		}
	}
	url.searchParams.set("absolute_timespan_cusps", absoluteTimespanCusps);
	if(specificColumn == null) {
		specificColumn = "";
	}
	if(!scale){
		scale = "day";
	}
	let deviceIdDropdown = document.getElementById('locationDropdown');
	if(!justLoaded){
		deviceId  = deviceIdDropdown[deviceIdDropdown.selectedIndex].value;
		absoluteTimeAgo = "";
	}  
	if(!deviceId){
		deviceId = defaultdeviceId;
	}
	url.searchParams.set("location_id", deviceId);
	if(deviceIds == null || !deviceIds) {
		deviceIds = "";//deviceId; //nope!
		//if(!justLoaded){
			//deviceIds = deviceId;
		//}
	}
	if(document.getElementById('scaleDropdown')  && !justLoaded){
		scale = document.getElementById('scaleDropdown')[document.getElementById('scaleDropdown').selectedIndex].value;
	}	
	url.searchParams.set("scale", scale);

	let specificColumnSelect = document.getElementById('specific_column');
	if(!justLoaded){
		specificColumn = document.getElementById('specific_column')[document.getElementById('specific_column').selectedIndex].value;
	} else {
		if(specificColumnSelect) {
			let index = 0;
			for(let option of specificColumnSelect.options) {
				if (option == specificColumn){
					specificColumnSelect.selectedIndex = index;
				}
				index++;
			}
		}
	}
	url.searchParams.set("specific_column", specificColumn);
	
	let specificDevices = document.getElementsByName('specificDevice');
	if(!justLoaded){
		deviceIds = "";
		for(let device of specificDevices){
			if(device.checked){
				deviceIds += device.value + ",";
			}
		}
		deviceIds = deviceIds.slice(0, -1);
	} else {
		deviceIdArray = deviceIds.split(",");
		for(let device of specificDevices){
			if(deviceIdArray.includes(device.value)){ // || plotType != 'multi' && device.value == deviceId
				device.checked = true;
			}
		}
	}
	deviceIdArray = deviceIds.split(",");
	url.searchParams.set("location_ids", deviceIds);
	//make the startDateDropdown switch to the appropriate item on the new scale:
	let periodAgoDropdown = document.getElementById('startDateDropdown');	

	if(periodAgoDropdown){
		if(!justLoaded){
			periodAgo = periodAgoDropdown[periodAgoDropdown.selectedIndex].value;
		} else {
			periodAgo = 0;
		}
		if(currentStartDate == periodAgoDropdown[periodAgoDropdown.selectedIndex].text){
			thisPeriod = periodAgo;
			periodAgo = false;
			//console.log("dates unchanged");
		}
		currentStartDate = periodAgoDropdown[periodAgoDropdown.selectedIndex].text;
    if(!justLoaded){
      absoluteTimeAgo = currentStartDate; //hmm, we can do this instead now
      url.searchParams.set("absolute_time_ago", absoluteTimeAgo);
    }
	}	
	periodAgo = calculateRevisedTimespanPeriod(scaleConfig, periodAgo, scale, currentStartDate);
	url.searchParams.set("period_ago", periodAgo);
	if(!yearsAgo){
		//console.log(deviceIdArray);
		resetGraphData(deviceIdArray);
	} 
	//update the URL without changing actual location
	history.pushState({}, "", url);
	let xhttp = new XMLHttpRequest();
	/*
	let endpointUrl = "./data.php?scale=" + scale + "&period_ago=" + periodAgo + "&mode=getWeatherData&deviceId=" + deviceId + "&absolute_timespan_cusps=" + absoluteTimespanCusps + "&years_ago=" + yearsAgo;
	if(absoluteTimeAgo) {
    endpointUrl = "./data.php?scale=" + scale + "&absolute_time_ago=" + absoluteTimeAgo + "&mode=getWeatherData&deviceId=" + deviceId + "&absolute_timespan_cusps=" + absoluteTimespanCusps + "&years_ago=" + yearsAgo;
	}
	*/
  let allData = 0;
	let liveData = 0;
	let maxRecorded = "";
	let endpointUrl = backendDataUrl("getWeatherData", deviceId, allData, liveData, maxRecorded, scale, absoluteTimespanCusps, periodAgo, absoluteTimeAgo, yearsAgo);
	
	if(plotType == 'multi'){
		document.getElementById("singleplotdiv").style.opacity ='0.5';
		document.getElementById("multiplotdiv").style.opacity ='1';
		endpointUrl += "&specific_column=" + specificColumn + "&location_ids=" + deviceIds;
		deviceIdDropdown.disabled = true;
		specificColumnSelect.disabled = false;
		for(let specificDevice of specificDevices) {
			specificDevice.disabled = false;
		}
	} else {
		document.getElementById("singleplotdiv").style.opacity ='1';
		document.getElementById("multiplotdiv").style.opacity ='.5';;
		deviceIdDropdown.disabled = false;
		specificColumnSelect.disabled = true;
		for(let specificDevice of specificDevices) {
			specificDevice.disabled = true;
		}
	}
	//console.log(endpointUrl);
	xhttp.onreadystatechange = function() {
	    if (this.readyState == 4 && this.status == 200) {
	     //Push the data in array
			timeStamp = [];

			//for(let specificdeviceId of deviceIdArray){
			//	multiGraphDataObject[yearsAgo][specificdeviceId] = {"values": [], "timeStamps": []};
			//}
			
			let time = new Date().toLocaleTimeString();
			//console.log(this.responseText);
			let dataObject = JSON.parse(this.responseText); 
			//let tbody = document.getElementById("tableBody");
			//tbody.innerHTML = ''
			//if(yearsAgo > 0){  
				//console.log(dataObject);
			//}
			if(dataObject) {
				if(dataObject[0] && dataObject[0]["sql"]){
					//console.log(dataObject[0]["sql"], dataObject[0]["error"]);
				} else {
					devices = dataObject["devices"]; //for proper labels in the graph
					const locationsForColumnsWeCareAbout = [...deviceIdArray];
					locationsForColumnsWeCareAbout.push(deviceId.toString());
					updateColumnsWeCareAbout(devices,  locationsForColumnsWeCareAbout);
					//console.log(devices);
					for(let datum of dataObject["records"]) {
						let time = datum["recorded"];
						if(plotType == "multi") {
;							deviceId = datum["device_id"];
							let value = datum[specificColumn];
							dataInvalid = false;
							if(specificColumn == "temperature"){

								value = value * (9/5) + 32;
							}
							if(multiGraphDataObject[yearsAgo][deviceId]) {
                multiGraphDataObject[yearsAgo][deviceId]["values"].push(value);
								for(let specificdeviceId of deviceIdArray){
									if(specificdeviceId != deviceId){
										multiGraphDataObject[yearsAgo][specificdeviceId]["values"].push(null);
									}
								}
								multiGraphDataObject[yearsAgo][deviceId]["timeStamps"].push(time);
								if(time> greatestTime) {
									greatestTime = time;
								}
							}
						} else {
							//graphDataObject[year][column]
							for (let column of columnsWeCareAbout){
								let mapToUse;
								let foundDevice = findObjectByColumn(devices, "device_id", deviceId.toString());
								let deviceColumnMaps = foundDevice["device_column_maps"];
								for(let map of deviceColumnMaps){
									if(column == map["column_name"] && map["table_name"] == "device_log"){
										mapToUse = map;
									}
								}
								let value = datum[column];

 
								if(mapToUse && mapToUse["view_function"]) {
									//process the string using the view_function if it's for post-processing
									let evalString = tokenReplace(mapToUse["view_function"], datum,  "<", "/>");
									value = eval(evalString);
								}
	 
								if(column == "temperature"){

									value = value * (9/5) + 32;
								}
								if(!graphDataObject[yearsAgo]){
									graphDataObject[yearsAgo] = [];
								}
								if(!graphDataObject[yearsAgo][column]){
									graphDataObject[yearsAgo][column] = [];
								}
                graphDataObject[yearsAgo][column].push(value);
							}
							timeStamp.push(time)
							if(time> greatestTime) {
								greatestTime = time;
							}
						}	
					}
				}
				//crudely make the old data have the same number of items as the new data
				if(yearsAgo > 0) {
					if(plotType == "multi") {
						multiGraphDataObject = fillOutArray(multiGraphDataObject, yearsAgo, deviceIdArray, "values");
						//console.log(multiGraphDataObject);
					} else{
						graphDataObject = fillOutArray(graphDataObject, yearsAgo, columnsWeCareAbout, null);
						
					}
				}



			}
			
			if(yearsAgo == 0){
				glblChart = showGraph(deviceId, plotType);  //Update Graphs
				document.getElementById('greatestTime').innerHTML = " Latest Data: " + timeAgo(greatestTime);
			} else {
				addPastYearToGraph(deviceIdArray, deviceId, yearsAgo, plotType);
			}
			officialWeather(deviceId);
			if(yearsAgoToShow){
				//console.log(deviceIdArray, yearsAgoToShow, plotType);
				if(pastYearsViewed.indexOf(parseInt(yearsAgoToShow)) < 0){
					getWeatherData(yearsAgoToShow);
				}
			}
	    }
		document.getElementsByClassName("outercontent")[0].style.backgroundColor='#ffffff';
	  };
  xhttp.open("GET", endpointUrl, true); 
  xhttp.send();
  createTimescalePeriodDropdown(scaleConfig, periodAgo, scale, currentStartDate, 'change', 'getWeatherData(0)', 'device_log', deviceId);
  
  justLoaded = false;
}

function fillOutArray(rootArrayToFillOut, yearsAgo, arrayOfSpecialItem, possibleKey){
	for (let item of arrayOfSpecialItem){
		let arrayWeCareAbout = rootArrayToFillOut[yearsAgo][item];
		let zerothArrayWeCareAbout = rootArrayToFillOut[0][item];
		
		if(possibleKey){
			arrayWeCareAbout = rootArrayToFillOut[yearsAgo][item][possibleKey];
			zerothArrayWeCareAbout = rootArrayToFillOut[0][item][possibleKey];
		} 
		let yearDifferenceDelta = arrayWeCareAbout.length - zerothArrayWeCareAbout.length;
		if(yearDifferenceDelta > 0){
			//arrayWeCareAbout.splice(-yearDifferenceDelta); //remove extra items.  crude, but close enough
		}
		//better than just repeating the last data point to fill out the graph or throwing away items at the end
		arrayWeCareAbout = expandArray(arrayWeCareAbout, -yearDifferenceDelta);
		//console.log("year ago length after:" + arrayWeCareAbout.length);
		if(possibleKey){
			rootArrayToFillOut[yearsAgo][item][possibleKey] = arrayWeCareAbout;
		} else {
			rootArrayToFillOut[yearsAgo][item] = arrayWeCareAbout;
		}
	}
	return rootArrayToFillOut;
}

function ctof(inVal){
	return inVal * (9/5) + 32;
}

function officialWeather(deviceId) {
	let xhttp = new XMLHttpRequest();
	let endpointUrl = "./data.php?mode=getOfficialWeatherData&deviceId=" + deviceId;
	xhttp.onreadystatechange = function() {
	    if (this.readyState == 4 && this.status == 200) {
	     //Push the data in array
			 
			//console.log(this.responseText);
			let dataObject = JSON.parse(this.responseText); 
			let weatherDescription = dataObject["official_weather"]["weather"][0]["description"];
			let temperature = dataObject["official_weather"]["main"]["temp"];
			let temperatureMin = dataObject["official_weather"]["main"]["temp_min"];
			let temperatureMax = dataObject["official_weather"]["main"]["temp_max"];
			let pressure = dataObject["official_weather"]["main"]["pressure"];
			let humidity = dataObject["official_weather"]["main"]["humidity"];
			let wind = dataObject["official_weather"]["wind"]["speed"];
			let gust = dataObject["official_weather"]["wind"]["gust"];
			let sunrise = dataObject["official_weather"]["sys"]["sunrise"];
			let sunset = dataObject["official_weather"]["sys"]["sunset"];
			let weatherdiv = document.getElementById("officialweather");
			let out = "<table>";
			//out += "<tr><td>Location</td><td>" + deviceId + "</td></tr>\n";
			out += "<tr><td>Weather</td><td>" + weatherDescription + "</td></tr>\n";
			out += "<tr><td>Temperature</td><td>" + ctof(temperature).toFixed(1); + "</td></tr>\n";
			out += "<tr><td>Extremes</td><td>" + ctof(temperatureMin).toFixed(1); + " to " + ctof(temperatureMax).toFixed(1); + "</td></tr>\n";
			out += "<tr><td>Pressure</td><td>" + pressure.toFixed(1); + "</td></tr>\n";
			out += "<tr><td>Humidity</td><td>" + humidity.toFixed(1); + "</td></tr>\n";
			out += "<tr><td>Wind</td><td>" + wind.toFixed(1); + "</td></tr>\n";
			//out += "<tr><td>Gust</td><td>" + gust.toFixed(1); + "</td></tr>\n"; //i guess they got rid of this one
			out += "<tr><td>Sunrise</td><td>" + timeConverter(sunrise) + "</td></tr>\n";
			out += "<tr><td>Sunset</td><td>" + timeConverter(sunset) + "</td></tr>\n";
			out += "</table>";
			weatherdiv.innerHTML = out;
 
	    }
	  };
  xhttp.open("GET", endpointUrl, true); //Handle getData server on ESP8266
  xhttp.send();
}

function updateColumnsWeCareAbout(devices, deviceIds){
	columnsWeCareAbout = [];
	for(const device of devices){
		if(device["device_column_maps"]) {
			if(deviceIds.indexOf(device["device_id"])>-1){
				for(const map of device["device_column_maps"]){
					const columnName = map["column_name"];
					if(columnsWeCareAbout.indexOf(columnName) < 0){
						columnsWeCareAbout.push(map["column_name"]);
					}		
				}
			}
		}
	}
}

function editColumns(){
	const queryParams = new URLSearchParams(window.location.search);
	const deviceId = queryParams.get('location_id');
	window.location = "tool.php?table=device_column_map&device_id=" + deviceId;
}

getWeatherData();