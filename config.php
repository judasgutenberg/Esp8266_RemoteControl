<?php

$servername = "localhost";
$username = "remote";
$database = "remote";
$password = "your_awesome_password";
$salt = "another random string";  //i don't think we use this
$encryptionScheme = '1234567890ABCDEF'; //needs to match ci[ENCRYPTION_SCHEME] in the firmware
$timezone = "America/New_York";
$encryptionPassword = "your secret just for you";//needs to match ci[STORAGE_PASSWORD] in the firmware
$cookiename = "email";
$poserCookieName = "poser_id";
$tenantCookieName = "tenant";
$backupLocation = "./someplace_secret_or_perhaps_not_in_your_web_tree";

//these are for cases where you need some other server to provide send email functionality using an externalmailer
$remoteEmailPassword = "your_external_mailer_password";
$remoteEmailUrl = "http://your_other_domain.com/externalmailer.php";
$flash_directory = "another_place_that_is_secret_or_perhaps_not_in_your_web_tree"
