import wave, struct, math, random, csv
from playsound import playsound
import serial
import datetime

wav_data = []

# record_seconds = 10 # record total seconds

# estimate_data_size = 10 * 16000

# data_read = 0




# with serial.Serial('/dev/ttyACM0', 115200, timeout=100) as ser:
#     while data_read < estimate_data_size:
#         line = ser.readline()   # read a '\n' terminated line
        
#         wav_data.append(int(line))
#         data_read = data_read + 1




# max_val = max(wav_data)

# min_val = min(wav_data)

# max_val = max(max_val, abs(min_val))
# x = datetime.datetime.now

sound_file_name = 'sdcard.wav'

sampleRate = 16000.0 # hertz
duration = 1.0 # seconds
frequency = 440.0 # hertz
obj = wave.open(sound_file_name,'w')
obj.setnchannels(1) # mono
obj.setsampwidth(2)
obj.setframerate(sampleRate)


with open("TEST15.bin","rb") as f:
    while 1:
        byte = f.read(2)

        if not byte:
            break
        else:
            # wav_data.append(struct.unpack('<h', byte)[0])
            obj.writeframesraw(byte)


# for i in wav_data:
#    j = int((i / max_val) * 32767)
#    data = struct.pack('<h', j)
#    obj.writeframesraw( data )
obj.close()

playsound(sound_file_name)