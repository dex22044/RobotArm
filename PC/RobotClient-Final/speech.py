import speech_recognition as sr
from fuzzywuzzy import fuzz

r = sr.Recognizer()

cmdList = [
    'отпусти губку',
    'возьми губку',
    'отпусти болт',
    'возьми болт',
    'отпусти маркер',
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

            print(ansIdx, flush=True, end='')
        except sr.UnknownValueError:
            #print('шта?', flush=True)
            pass