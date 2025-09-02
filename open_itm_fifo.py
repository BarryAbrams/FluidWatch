Import("env")
import os

# Path where OpenOCD will dump SWO ITM data
fifo_path = os.path.join(env["PROJECT_BUILD_DIR"], "itm.fifo")

def before_debug(source, target, env):
    if not os.path.exists(fifo_path):
        os.mkfifo(fifo_path)

env.AddPreAction("debug", before_debug)
