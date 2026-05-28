#pragma once

// Start WiFi SoftAP ("TiVoTranslator" / "tivotivo") and HTTP server on port 80.
void webConfigInit();

// Call from loop() to service HTTP requests.
void webConfigLoop();
