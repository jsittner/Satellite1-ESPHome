import pyaudio
import socket
import wave
import os
from datetime import datetime

def get_local_ip() -> str:
    try:
        # Use a dummy connection to a known public IP address to determine the local IP
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            # Connect to an external IP and port (Google's public DNS in this case)
            s.connect(("8.8.8.8", 80))
            local_ip = s.getsockname()[0]  # Get the local IP used for the connection
        return local_ip
    except Exception as e:
        print(f"Error determining local IP: {e}")
        return None


"""
Listen on udp port 6055 for audio data and stream directly to output speaker.
"""
FORMAT = pyaudio.paInt16  # Format of sampling
CHANNELS = 1              # Number of audio channels (1 for mono, 2 for stereo)
RATE = 16000              # Sampling rate (samples per second)
CHUNK = 512              # Number of audio frames per buffer
RECORD_SECONDS = 10       # Duration of recording
PORT = 6055

"""
Store records under test_runs/CURRENT_DATETIME
"""
ROOT_DIR = os.path.join( os.path.dirname(__file__), "../../")
TEST_RUN_ROOT = os.path.join(ROOT_DIR, "testdata", "mic_streaming" )
TEST_RUN_DIR = os.path.join(TEST_RUN_ROOT, datetime.now().isoformat() )
os.makedirs( TEST_RUN_DIR )

print( get_local_ip() )
print( f"Additional streaming delay: { CHUNK / RATE * 1000  } ms" )

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 3 * CHUNK)
sock.bind(("0.0.0.0", PORT))

# Create an audio object
p = pyaudio.PyAudio()

# Open stream
stream = p.open(format=pyaudio.paInt16, channels=CHANNELS, rate=RATE, output=True)

file_idx = 0
chunks = []
if 1:
    while True:
        try:
            data, addr = sock.recvfrom(CHUNK)
            stream.write(data)
        except KeyboardInterrupt:
            break
else:
    while True:
        try:
            data, addr = sock.recvfrom(CHUNK)
            chunks.append(data)
            if len(chunks) == int(RATE / CHUNK * RECORD_SECONDS) :
                with wave.open( os.path.join( TEST_RUN_DIR, f"rec_{file_idx:03d}.wav"), 'wb') as wf:
                    wf.setnchannels(CHANNELS)
                    wf.setsampwidth(p.get_sample_size(FORMAT))
                    wf.setframerate(RATE)
                    wf.writeframes(b''.join(chunks))
                    chunks = []
                    file_idx += 1
                chunks = []
            stream.write(data)
        except KeyboardInterrupt:
            break

stream.stop_stream()
stream.close()
p.terminate()

sock.close()


