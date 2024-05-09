import re

print("Will read file")

filetokens = ""


result = ""
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

print (result)