import re
import pyperclip

print("Will read HTML file and convert to C String...\n")

filetokens = ""



result = chr(34);
with open('index.html', 'r') as fileobj:
    for row in fileobj:
        token = row.rstrip('\n')
        #quotePosition = token.find('"')
        #if quotePosition != -1:
        #    print("Quote found!")
        token = token.replace('"',"dan")
        token = token.replace('dan',chr(92)+chr(34))

       
        tmpToken = re.sub('   +' , ' ', token)


        
        result = result + token
  #      print(row.rstrip('\n'))

result = result +  chr(34)  + ';'


pyperclip.copy(result)
spam = pyperclip.paste()

print (result)

print("\nThe C String is copied into Window's paperclick, ready to past into your c/cpp file")