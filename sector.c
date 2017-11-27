#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "sector.h"
#include "fat16.h"

const int LENGTH_SECTOR = 512; // tamanho em bytes do setor

//dado o descritor do arquivo e o numero do setor, le o setor e o coloca no buffer
/*	parametros
	
	fd -> descritor do arquivo
	secnum -> numero do setor a ser lido
	buffer -> buffer ser gravado
*/
void sector_read(FILE *fd,  int secnum, void *buffer)
{
	//log_msg("vou ler o sector: %d na posicao do arquivo: %d\n",secnum,secnum * LENGTH_SECTOR);
	
	int seek = fseek(fd, LENGTH_SECTOR * secnum , SEEK_SET);
	if(seek)//caso nao foi possivel fazer a operação de seek
	{
		exit(EXIT_FAILURE);
	}
	
	int read = fread(buffer , LENGTH_SECTOR ,1, fd);
	
	if(read != 1)//caso nao foi possivel ler 1 espaço de memoria de 512 bytes do arquivo
	{
		exit(EXIT_FAILURE);
	}

}
