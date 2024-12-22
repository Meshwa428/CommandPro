# Wait Commands
WAIT 2s;
WAIT 1.5h;
WAIT 30m;
WAIT 500ms;

# Scheduling
RUN AT "10:45 AM" {
    OPEN APP "Browser";
    WRITE "https://example.com";
}

INTERVAL 10m {
    CAPTURE SCREEN INTO "screenshot.png";
}

# Time Variables
SET longWait = 1.5h;
SET shortWait = 500ms;
WAIT longWait;
WAIT shortWait;