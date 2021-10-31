import speech_recognition as sr
from fuzzywuzzy import fuzz

r = sr.Recognizer()

cmdList = [
    'верни губку',
    'возьми губку',
    'верни болт',
    'возьми болт',
    'верни маркер',
    'возьми маркер'
]

with sr.Microphone(device_index=11) as source:
    r.adjust_for_ambient_noise(source, duration=3)
    while True:
        try:
            #print('аахахахахахха', flush=True)
            audio = r.listen(source)

            query = r.recognize_google(audio, language='ru-RU').lower()
            maxRatio = 0
            ans = ''
            ansIdx = -1
            for index, cmd in enumerate(cmdList):
                rat = fuzz.ratio(cmd, query)
                if rat > maxRatio:
                    maxRatio = rat
                    ans = cmd
                    ansIdx = index

            if ansIdx != -1:
                print(ansIdx, flush=True, end='')
        except sr.UnknownValueError:
            #print('шта?', flush=True)
            pass