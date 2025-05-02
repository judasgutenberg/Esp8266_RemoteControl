<?php

$servername = "localhost";
$username = "remote";
$database = "remote";
$password = "your_awesome_password";
$salt = "another random string";
$encryptionScheme = '1234567890ABCDEF';

$timezone = "America/New_York";
$encryptionPassword = "your secret just for you";
$cookiename = "email";
$poserCookieName = "poser_id";
$tenantCookieName = "tenant";
$backupLocation = "./someplace_secret_or_perhaps_not_in_your_web_tree";

//these are for cases where you need some other server to provide send email functionality using an externalmailer
$remoteEmailPassword = "your_external_mailer_password";
$remoteEmailUrl = "http://your_other_domain.com/externalmailer.php";
