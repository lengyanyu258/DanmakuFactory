CArg = ${CARG}
OutPut = DanmakuFactory
CC = gcc $(CArg)
Object = String/DanmakuFactoryString.o List/DanmakuFactoryList.o Config/Config.o TemplateFile/TemplateFile.o AssFile/AssStringProcessing.o AssFile/AssFile.o XmlFile.o JsonFile.o main.o

$(OutPut): $(Object)
	$(CC) -o $(OutPut) $(Object)

String/DanmakuFactoryString.o: String/DanmakuFactoryString.c
List/DanmakuFactoryList.o: List/DanmakuFactoryList.c
Config/Config.o: Config/Config.c
TemplateFile/TemplateFile.o: TemplateFile/TemplateFile.c
AssFile/AssStringProcessing.o: AssFile/AssStringProcessing.c
AssFile/AssFile.o: AssFile/AssFile.c
XmlFile.o: XmlFile.c
JsonFile.o: JsonFile.c
main.o: main.c

.PHONY: clean
clean:
	-rm $(OutPut) $(Object)