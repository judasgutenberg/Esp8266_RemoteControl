<?php
include("config.php");
include("site_functions.php");
include("device_functions.php");


$conn = mysqli_connect($servername, $username, $password, $database);
$user = autoLogin();
$action = gvfw("action");
if ($action == "login") {
	$tenantId = gvfa("tenant_id", $_GET);
	$tenantSelector = loginUser($tenantId);
} else if ($action == 'settenant') {
	setTenant(gvfw("encrypted_tenant_id"));
} else if ($action == "logout") {
	logOut();
	header("Location: ?action=login");
	die();
}
if(!$user) {
	if(gvfa("password", $_POST) != "" && $tenantSelector == "") {
		$content .= "<div class='genericformerror'>The credentials you entered have failed.</div>";
	}
    if(!$tenantSelector) {
		$content .= loginForm();
	} else {
		$content .= $tenantSelector;
	}
	echo bodyWrap($content, $user, "", null);
	die();
}
$deviceId = gvfw("device_id", 1);
$daysToGet = gvfw("days", 1);
$device = getDevice($deviceId);
 
//var_dump(getWeatherForecastByCoordinates($device["latitude"], $device["longitude"], $daysToGet));
//saveWeatherForecastsForDay($user);
//var_dump(getHourlySolarProduction("2026-06-24", $user));
updateForecastSolarRecordsWithPowerValues($user, 2);