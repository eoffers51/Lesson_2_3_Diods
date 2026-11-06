set pagination off
target remote :3333

# Boot the chip under OpenOCD's control. Apptrace over JTAG only delivers data
# when the app started while the debugger was attached: attaching to an app
# that booted on its own gives "apptrace connection is lost. Re-connect." and
# "Invalid init seq len" in the OpenOCD log and an empty trace.
mon reset run

# Give the app 3 s to boot and create its tasks (ping as a sleep on Windows;
# on Linux/macOS use: shell sleep 3).
shell ping -n 4 127.0.0.1 > NUL

# Start recording on the running app. The firmware answers START with the
# system description and the task list, so every task is named. The file path
# is relative to the folder OpenOCD was started in (the project root with
# "idf.py openocd"); an absolute path works too.
# The trailing "1" is the poll period in ms = non-blocking mode. Without it
# OpenOCD v0.12.0-esp32-20260424 runs the command in blocking mode (the log says
# "poll period 0 ms"), ignores "stop", and never writes the merged .SVDat
# (only trace.SVDat_core0/_core1 grow).
mon esp sysview_mcore start file://build_trace/trace.SVDat 1

# The chip keeps running and gdb shows its prompt. Record as long as you like,
# then type:
#   mon esp sysview_mcore stop
#   quit          (answer y)
#
# Why not ESP-IDF's example gdbinit ("mon reset halt", "b app_main", start in
# the breakpoint's command list): on a dual-core ESP32-S3 that start lost part
# of core 0's start sequence (OpenOCD logged "CRC mismatch" retries) and
# SystemView then showed core 1 only. Starting on the running app gave complete
# streams for both cores every time.
