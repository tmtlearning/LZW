#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BITS 12                   /* Установка длины кода   */
#define HASHING_SHIFT (BITS-8)    /* и размера таблицы строк    */
#define MAX_VALUE (1 << BITS) - 1 
#define MAX_CODE MAX_VALUE - 1    
#define TABLE_SIZE 5021           

void *malloc();

int *code_value;                  /*  Массив значений кодов    */
unsigned int *prefix_code;        /*  Массив префиксов кодов */
unsigned char *append_character;  /*  Массив добавочных символов */
unsigned char decode_stack[4000]; /*  Массив декодируемых строк */

/*
 * Прототипы функций
 */
void compress(FILE *input,FILE *output);
void expand(FILE *input,FILE *output);
int find_match(int hash_prefix,unsigned int hash_character);
void output_code(FILE *output,unsigned int code);
unsigned int input_code(FILE *input);
unsigned char *decode_string(unsigned char *buffer,unsigned int code);

/********************************************************************
**
** Программа получает имя файла из командной строки, сжимает его, помещая результат в файл compressed.txt, 
** а затем производит декомпрессию с занесением результата в файл test.txt.
** test.txt должен быть копией входного файла
** 
*************************************************************************/

int main(int argc, char *argv[])
{
FILE *input_file;
FILE *output_file;
FILE *lzw_file;
char input_file_name[81];

/*
**  Выделяем адресное пространство 
*/
  code_value=(int*)malloc(TABLE_SIZE*sizeof(int));
  prefix_code=(unsigned int *)malloc(TABLE_SIZE*sizeof(unsigned int));
  append_character=(unsigned char *)malloc(TABLE_SIZE*sizeof(unsigned char));
  if (code_value==NULL || prefix_code==NULL || append_character==NULL)
  {
    printf("Ошибка выделения памяти!\n");
    exit(-1);
  }
/*
** Получаем имя файла для сжатия и открываем выходной файл.
*/
  if (argc>1)
    strcpy(input_file_name,argv[1]);
  else
  {
    printf("Введите имя файла? ");
    scanf("%s",input_file_name);
  }
  input_file=fopen(input_file_name,"rb");
  lzw_file=fopen("compressed.txt","wb");
  if (input_file==NULL || lzw_file==NULL)
  {
    printf("Ошибка открытия файла.\n");
    exit(-1);
  };
/*
** Сжимаем файл.
*/
  compress(input_file,lzw_file);
  fclose(input_file);
  fclose(lzw_file);
  free(code_value);
/*
** Открываем файла для распаковки.
*/
  lzw_file=fopen("compressed.txt","rb");
  output_file=fopen("test.txt","wb");
  if (lzw_file==NULL || output_file==NULL)
  {
    printf("Ошибка открытия файла.\n");
    exit(-2);
  };
/*
** Распаковываем файл.
*/
  expand(lzw_file,output_file);
  fclose(lzw_file);
  fclose(output_file);

  free(prefix_code);
  free(append_character);
  
  return 0;
}

/*
** Процедура сжатия, за описанием алгоритма обращайтесь в wiki.
*/

void compress(FILE *input,FILE *output)
{
unsigned int next_code;
unsigned int character;
unsigned int string_code;
unsigned int index;
int i;

  next_code=256;              /* Следующий доступный код*/
  for (i=0;i<TABLE_SIZE;i++)  /* Инициализируем таблицу кодов */
    code_value[i]=-1;

  i=0;
  string_code=getc(input);    /* Получаем первый код  */
/*
** Основной цикл, который работает, пока остались символы во входном потоке
** Цикл прекращает работу, когда все возможные коды были определены
*/
  while ((character=getc(input)) != (unsigned)EOF)
  {
    index=find_match(string_code,character);/* Проверяем, есть ли строка в таблице. */
    if (code_value[index] != -1)            /* Если есть, то берем значение соответствующего кода, */
      string_code=code_value[index];        /* в противном случае добавляем код в таблицу */
    else                                    
    {                                       
      if (next_code <= MAX_CODE)
      {
        code_value[index]=next_code++;
        prefix_code[index]=string_code;
        append_character[index]=character;
      }
      output_code(output,string_code);  /* Если строки нет в таблице,  */
      string_code=character;            /* то выводим последнюю */
    }                                   /* перед добавлением новой */
  }                                     
/*
** Конец главного цикла
*/
  output_code(output,string_code); /* Выводим последний код */
  output_code(output,MAX_VALUE);   /* Вывод признака конца потока */
  output_code(output,0);           /* Очищаем выходной буфер */
  printf("\n");
}

/*
** Процедура хэширования.  Она пытается найти сопоставление для строки
** префикс+символ в таблице строк. Если найдено, возвращается индекс.
** Если нет, то возвращается первый доступный индекс.
** Всё очень сложно.
*/

int find_match(int hash_prefix,unsigned int hash_character)
{
int index;
int offset;

  index = (hash_character << HASHING_SHIFT) ^ hash_prefix;
  if (index == 0)
    offset = 1;
  else
    offset = TABLE_SIZE - index;
  while (1)
  {
    if (code_value[index] == -1)
      return(index);
    if (prefix_code[index] == hash_prefix && 
        append_character[index] == hash_character)
      return(index);
    index -= offset;
    if (index < 0)
      index += TABLE_SIZE;
  }
}

/*
**  Процедура распаковки. Читаем сжатый файл и распаковывает
**  его в выходной файл.
*/

void expand(FILE *input,FILE *output)
{
unsigned int next_code;
unsigned int new_code;
unsigned int old_code;
int character;
unsigned char *string;

  next_code=256;           /* Следующий доступный код */

  old_code=input_code(input);  /* Читаем первый код, инициализируем переменную */
  character=old_code;          /* character и посылаем первый символ в выходной файл  */
  putc(old_code,output);       
/*
**  Основной цикл распаковки. Читаются коды из сжатого файла до тех пор,
**  пока не встретится специальный код, указывающий на конец данных.
*/
  while ((new_code=input_code(input)) != (MAX_VALUE))
  {
/*
** Проверка кода для специального случая 
** STRING+CHARACTER+STRING+CHARACTER+STRING,
** когда генерируется неопределенный код.
** Это заставляет его декодировать последний код, 
** добавив CHARACTER в конец декод. строки.
*/
    if (new_code>=next_code)
    {
      *decode_stack=character;
      string=decode_string(decode_stack+1,old_code);
    }
/*
** Иначе декодируется новый код
*/
    else
      string=decode_string(decode_stack,new_code);
/*
** Выводим декодируемую строку в обратном порядке
*/
    character=*string;
    while (string >= decode_stack)
      putc(*string--,output);
/*
** Если возможно, добавляем новый код в таблицу 
*/
    if (next_code <= MAX_CODE)
    {
      prefix_code[next_code]=old_code;
      append_character[next_code]=character;
      next_code++;
    }
    old_code=new_code;
  }
  printf("\n");
}

/*
** Процедура простого декодирования строки из таблицы строк,
* сохраняющая
** результат в буфер.  Этот буфер потом может быть выведен
** в обратном порядке функцией распаковки.
*/

unsigned char *decode_string(unsigned char *buffer,unsigned int code)
{
int i;

  i=0;
  while (code > 255)
  {
    *buffer++ = append_character[code];
    code=prefix_code[code];
    if (i++>=MAX_CODE)
    {
      printf("Fatal error during code expansion.\n");
      exit(-3);
    }
  }
  *buffer=code;
  return(buffer);
}

/*
** Следующие две процедуры управляют вводом/выводом кодов
** переменной длины.
** Всё очень сложно.
*/

unsigned int input_code(FILE *input)
{
unsigned int return_value;
static int input_bit_count=0;
static unsigned long input_bit_buffer=0L;

  while (input_bit_count <= 24)
  {
    input_bit_buffer |= 
        (unsigned long) getc(input) << (24-input_bit_count);
    input_bit_count += 8;
  }
  return_value=input_bit_buffer >> (32-BITS);
  input_bit_buffer <<= BITS;
  input_bit_count -= BITS;
  return(return_value);
}

void output_code(FILE *output,unsigned int code)
{
static int output_bit_count=0;
static unsigned long output_bit_buffer=0L;

  output_bit_buffer |= (unsigned long) code << (32-BITS-output_bit_count);
  output_bit_count += BITS;
  while (output_bit_count >= 8)
  {
    putc(output_bit_buffer >> 24,output);
    output_bit_buffer <<= 8;
    output_bit_count -= 8;
  }
}
