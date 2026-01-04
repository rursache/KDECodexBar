import pty
import os
import time
import sys
import select

def read_all(fd):
    out = b""
    while True:
        r, _, _ = select.select([fd], [], [], 0.1)
        if not r: break
        try:
            chunk = os.read(fd, 10240)
            if not chunk: break
            out += chunk
        except OSError:
            break
    return out

pid, fd = pty.fork()
if pid == 0:
    # Child
    os.environ["TERM"] = "xterm-256color" # Ensure TUI runs
    os.execvp("gemini", ["gemini"])
else:
    # Parent
    print("Started gemini pid", pid)
    buffer = b""
    sent = False
    
    # Read loop
    start = time.time()
    while time.time() - start < 15: # 15s timeout
        chunk = read_all(fd)
        if chunk:
            buffer += chunk
            
        if not sent and (b"Tips for getting started" in buffer or b"/help" in buffer):
            print("Detected prompt, sending /stats")
            time.sleep(1) # Wait for settle
            os.write(fd, b"/stats\r")
            sent = True
            
        time.sleep(0.1)
        
    print("=== DUMP START ===")
    try:
        sys.stdout.buffer.write(buffer)
    except:
        print(buffer)
    print("=== DUMP END ===")
    
    try:
        os.kill(pid, 9)
    except:
        pass
