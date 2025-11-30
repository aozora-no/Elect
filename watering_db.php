<?php

$servername = "localhost";
$username   = "root";
$password   = "";
$dbname     = "watering_system";

header('Content-Type: application/json; charset=utf-8');

$conn = new mysqli($servername, $username, $password, $dbname);
if ($conn->connect_error) {
    http_response_code(500);
    echo json_encode(['success' => false, 'error' => 'DB connection failed: ' . $conn->connect_error]);
    exit;
}

$moisture_raw      = isset($_POST['moisture']) ? trim($_POST['moisture']) : '';
$moisture_level_in = isset($_POST['moisture_level']) ? trim($_POST['moisture_level']) : '';
$count_raw         = isset($_POST['count']) ? trim($_POST['count']) : '';


$moisture_level = strtolower($moisture_level_in);
if (!in_array($moisture_level, ['dry', 'wet'])) {
    $moisture_level = 'unknown';
    if ($moisture_raw !== '' && preg_match('/(-?\d+)/', $moisture_raw, $m)) {
        $percent = (int)$m[1];
        if ($percent <= 35) $moisture_level = 'dry';
        else $moisture_level = 'wet';
    }
}


$watered_count = 0;
if ($count_raw !== '') {
    $watered_count = (int)$count_raw;
    if ($watered_count < 0) $watered_count = 0;
}


if ($moisture_raw === '' && $moisture_level === 'unknown') {
    http_response_code(400);
    echo json_encode(['success' => false, 'error' => 'No moisture data provided']);
    $conn->close();
    exit;
}

$sql = "INSERT INTO wateringdb (moisture, moisture_level, watered_count) VALUES (?, ?, ?)";
$stmt = $conn->prepare($sql);
if (!$stmt) {
    http_response_code(500);
    echo json_encode(['success' => false, 'error' => 'Prepare failed: ' . $conn->error]);
    $conn->close();
    exit;
}

$moisture_for_db = $moisture_raw !== '' ? $moisture_raw : '';

$stmt->bind_param("ssi", $moisture_for_db, $moisture_level, $watered_count);

$exec = $stmt->execute();
if ($exec) {
    echo json_encode([
        'success' => true,
        'message' => 'Inserted',
        'insert_id' => $stmt->insert_id,
        'moisture_raw' => $moisture_for_db,
        'moisture_level' => $moisture_level,
        'watered_count' => $watered_count
    ]);
} else {
    http_response_code(500);
    echo json_encode(['success' => false, 'error' => 'Insert failed: ' . $stmt->error]);
}

$stmt->close();
$conn->close();
?>