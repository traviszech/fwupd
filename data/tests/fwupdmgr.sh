#!/bin/sh

exec 2>&1
device=08d460be0f1f9f128413f816022a6439e0078018

CAB="@installedtestsdir@/fakedevice123.cab"

error()
{
        rc=$1
        journalctl -u fwupd -b || true
        exit $rc
}

# ---
echo "Verify test device is present"
fwupdtool get-devices --json | jq -e '.Devices | any(.Plugin == "test")'
rc=$?; if [ $rc != 0 ]; then
    echo "Enable test device"
    fwupdtool enable-test-devices
    rc=$?; if [ $rc != 0 ]; then error $rc; fi
fi

# ---
echo "Show help output"
fwupdmgr --help
rc=$?; if [ $rc != 0 ]; then error $rc; fi

# ---
echo "Show version output"
fwupdmgr --version
rc=$?; if [ $rc != 0 ]; then error $rc; fi

# ---
echo "Getting the list of plugins..."
fwupdmgr get-plugins
rc=$?; if [ $rc != 0 ]; then error $rc; fi

# ---
echo "Getting the list of plugins (json)..."
fwupdmgr get-plugins --json
rc=$?; if [ $rc != 0 ]; then error $rc; fi

if [ -f ${CAB} ]; then
    # ---
    echo "Examining ${CAB}..."
    fwupdmgr get-details ${CAB}
    rc=$?; if [ $rc != 0 ]; then exit $rc; fi

    # ---
    echo "Examining ${CAB} (json)..."
    fwupdmgr get-details ${CAB} --json
    rc=$?; if [ $rc != 0 ]; then exit $rc; fi

    # ---
    echo "Installing ${CAB} cabinet..."
    fwupdmgr install ${CAB} --no-reboot-check
    rc=$?; if [ $rc != 0 ]; then exit $rc; fi
fi

# ---
echo "Update the device hash database..."
fwupdmgr get-releases $device
rc=$?; if [ $rc != 0 ]; then error $rc; fi

# ---
echo "Getting the list of remotes..."
fwupdmgr get-remotes
rc=$?; if [ $rc != 0 ]; then error $rc; fi

# ---
echo "Update the device hash database..."
fwupdmgr verify-update $device
rc=$?; if [ $rc != 0 ]; then error $rc; fi

# ---
echo "Getting devices (should be one)..."
fwupdmgr get-devices --no-unreported-check
rc=$?; if [ $rc != 0 ]; then error $rc; fi

# ---
echo "Testing the verification of firmware..."
fwupdmgr verify $device
rc=$?; if [ $rc != 0 ]; then error $rc; fi

# ---
echo "Getting updates (should be one)..."
fwupdmgr --no-unreported-check --no-metadata-check get-updates
rc=$?; if [ $rc != 0 ]; then error $rc; fi

# ---
echo "Installing test firmware..."
fwupdmgr update $device -y
rc=$?; if [ $rc != 0 ]; then error $rc; fi

# ---
echo "Check if anything was tagged for emulation"
fwupdmgr get-devices --json --filter emulation-tag | jq -e '(.Devices | length) > 0'
rc=$?; if [ $rc = 0 ]; then
    echo "Save device emulation"
    fwupdmgr emulation-save /dev/null
    rc=$?; if [ $rc != 0 ]; then error $rc; fi
    echo "Save device emulation (bad args)"
    fwupdmgr emulation-save
    rc=$?; if [ $rc != 1 ]; then error $rc; fi
fi

# ---
echo "Verifying results (str)..."
fwupdmgr get-results $device -y
rc=$?; if [ $rc != 0 ]; then error $rc; fi

# ---
echo "Verifying results (json)..."
fwupdmgr get-results $device -y --json
rc=$?; if [ $rc != 0 ]; then error $rc; fi

# ---
echo "Getting updates (should be none)..."
fwupdmgr --no-unreported-check --no-metadata-check get-updates
rc=$?; if [ $rc != 2 ]; then error $rc; fi

# ---
echo "Getting updates [json] (should be none)..."
fwupdmgr --no-unreported-check --no-metadata-check get-updates --json
rc=$?; if [ $rc != 0 ]; then error $rc; fi

# ---
echo "Testing the verification of firmware (again)..."
fwupdmgr verify $device
rc=$?; if [ $rc != 0 ]; then error $rc; fi

# ---
echo "Getting history (should be none)..."
fwupdmgr get-history
rc=$?; if [ $rc != 2 ]; then exit $rc; fi

if [ -z "$CI_NETWORK" ]; then
        echo "Skipping remaining tests due to CI_NETWORK not being set"
        exit 0
fi

# ---
echo "Downgrading to older release (requires network access)"
fwupdmgr --download-retries=5 downgrade $device -y
rc=$?; if [ $rc != 0 ]; then error $rc; fi

# ---
echo "Downgrading to older release (should be none)"
fwupdmgr downgrade $device
rc=$?; if [ $rc != 2 ]; then error $rc; fi

# ---
echo "Updating all devices to latest release (requires network access)"
fwupdmgr --download-retries=5 --no-unreported-check --no-metadata-check --no-reboot-check update -y
rc=$?; if [ $rc != 0 ]; then error $rc; fi

# ---
echo "Getting updates (should be none)..."
fwupdmgr --no-unreported-check --no-metadata-check get-updates
rc=$?; if [ $rc != 2 ]; then error $rc; fi

# ---
echo "Refreshing from the LVFS (requires network access)..."
fwupdmgr --download-retries=5 refresh
rc=$?; if [ $rc != 0 ]; then error $rc; fi

# success!
exit 0
