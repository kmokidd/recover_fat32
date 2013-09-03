#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define DEBUG	0
char name[12];
char dir[30]={"/"};
int rowNum = 0; 
int subRowNum = 0;
unsigned long tmpStartClustor = 0;
char oNumTable[6]={'a','b','c','d','e','f'};

//Copy from Sample Code
#pragma pack(push,1)
struct BootEntry
{
	unsigned char BS_jmpBoot[3];	/* Assembly instruction to jump to boot code */
	unsigned char BS_OEMName[8];	/* OEM Name in ASCII */
	unsigned short BPB_BytsPerSec; /* Bytes per sector. Allowed values include 512, 1024, 2048, and 4096 */
	unsigned char BPB_SecPerClus; /* Sectors per cluster (data unit). Allowed values are powers of 2, but the cluster size must be 32KB or smaller */
	unsigned short BPB_RsvdSecCnt;	/* Size in sectors of the reserved area */
	unsigned char BPB_NumFATs;	/* Number of FATs */
	unsigned short BPB_RootEntCnt; /* Maximum number of files in the root directory for FAT12 and FAT16. This is 0 for FAT32 */
	unsigned short BPB_TotSec16;	/* 16-bit value of number of sectors in file system */
	unsigned char BPB_Media;	/* Media type */
	unsigned short BPB_FATSz16; /* 16-bit size in sectors of each FAT for FAT12 and FAT16.  For FAT32, this field is 0 */
	unsigned short BPB_SecPerTrk;	/* Sectors per track of storage device */
	unsigned short BPB_NumHeads;	/* Number of heads in storage device */
	unsigned long BPB_HiddSec;	/* Number of sectors before the start of partition */
	unsigned long BPB_TotSec32; /* 32-bit value of number of sectors in file system.  Either this value or the 16-bit value above must be 0 */
	unsigned long BPB_FATSz32;	/* 32-bit size in sectors of one FAT */
	unsigned short BPB_ExtFlags;	/* A flag for FAT */
	unsigned short BPB_FSVer;	/* The major and minor version number */
	unsigned long BPB_RootClus;	/* Cluster where the root directory can be found */
	unsigned short BPB_FSInfo;	/* Sector where FSINFO structure can be found */
	unsigned short BPB_BkBootSec;	/* Sector where backup copy of boot sector is located */
	unsigned char BPB_Reserved[12];	/* Reserved */
	unsigned char BS_DrvNum;	/* BIOS INT13h drive number */
	unsigned char BS_Reserved1;	/* Not used */
	unsigned char BS_BootSig; /* Extended boot signature to identify if the next three values are valid */
	unsigned long BS_VolID;	/* Volume serial number */
	unsigned char BS_VolLab[11]; /* Volume label in ASCII. User defines when creating the file system */
	unsigned char BS_FilSysType[8];	/* File system type label in ASCII */
};

struct DirEntry
{
	unsigned char DIR_Name[11];
	unsigned char DIR_Attr;
	unsigned char DIR_NTRes;
	unsigned char DIR_CrtTimeTenth;
	unsigned short DIR_CrtTime;
	unsigned short DIR_CrtDate;
	unsigned short DIR_LstAccDate; 
	unsigned short DIR_FstClusHI; 
	unsigned short DIR_WrtTime;
	unsigned short DIR_WrtDate;
	unsigned short DIR_FstClusLO; 
	unsigned long DIR_FileSize;

};
#pragma pack(pop)

void help(){
	//printf("--------------dir:%s\n",dir);
	fprintf(stdout,"Usage: ./recover -d [device filename] [other arguments]\n");
	fprintf(stdout,"-i\t\t\tPrintf boot sector information\n");
	fprintf(stdout,"-l\t\t\tList all the directory entries\n");
	fprintf(stdout,"-r filename [-m md5]\tFile recovery\n");
}

//milestone 1, check arguments validation
int validArg(char argc, char *argv[]){
	if(argc<=5&&argc>=4){
		if(strcmp(argv[1],"-d")==0){ //the second arg should be -d
			if(fopen(argv[2],"r")!=NULL){ //the third arg should be the FAT32 disk
				//the forth arg should be -i or -l or -r
				if((strcmp(argv[3],"-i")==0)||(strcmp(argv[3],"-l")==0)||(strcmp(argv[3],"-r")==0)){ 
					if((strcmp(argv[3],"-r")==0)&&argc!=5){
						help();
						return -1;
					}
					if(argc==5){
						if((strcmp(argv[3],"-r")==0)){ // check the file exists or not
							return 0;
						}else{
							help();
							return -1;
						}	
					}else{
						return 0;
					}
				}else{
					help();
					return -1;
				}
			}else{
				help();
				return -1;
			}
		}else{
			help();
			return -1;
		}
	}else{
		help();
		return -1;
	}
}

//return a boot sector struct, let other functions to call it
struct BootEntry* readBoot(char *argv[]){
	FILE *fpTMP = fopen(argv[2], "r");
	struct BootEntry *be;
	char beTmp[512];

	if(fpTMP==NULL){
		fprintf(stdout, "Cannot open the file\n");
		return NULL;
	}

	fread(beTmp, sizeof(beTmp), 1, fpTMP);
	be = (struct BootEntry *)beTmp;
	return be;
}

//return data area start position
long GetFsector(char *argv[]){
	FILE *fp = fopen(argv[2], "r");
	struct BootEntry *be;
	char bufTmp[512];
	unsigned long skipNum = 0;

	if(fp==NULL){
		fprintf(stderr, "Cannot find the file.\n");
		exit(0);
	}

	fread(bufTmp, sizeof(bufTmp), 1, fp);
	be = (struct BootEntry *)bufTmp;
	skipNum = be->BPB_FATSz32*be->BPB_NumFATs*be->BPB_BytsPerSec;
	skipNum = skipNum+be->BPB_RsvdSecCnt*be->BPB_BytsPerSec;

	return skipNum;
}

//return fat table starting postion
long GetFatsector(char *argv[]){
	FILE *fp = fopen(argv[2], "r");
	struct BootEntry *be;
	char bufTmp[512];
	unsigned long skipNum = 0;

	if(fp==NULL){
		fprintf(stderr, "Cannot find the file.\n");
		exit(0);
	}

	fread(bufTmp, sizeof(bufTmp), 1, fp);
	be = (struct BootEntry *)bufTmp;
	skipNum =be->BPB_RsvdSecCnt*be->BPB_BytsPerSec;

	return skipNum;
}

//deal with space
void trim(char *buf,int flag){
	int tmp=0;
	for(;tmp<12;tmp++) //init
		name[tmp] = '\0';
	int i=0,j=0;

	if(flag>0){ //if input is a file
		for(i=0,j=0;i<8;i++){ //8 bytes filename
			if(buf[i]!=32){
				name[j] = buf[i];
				j++;
			}
		}
		name[j++] = '.'; 
		for(;i<11;i++){ //3 bytes extension 
			if(buf[i]==32)
				break;
			name[j] = buf[i];
			j++;
		}
		if(buf[0]==-27){//file need to be recovered
			for(tmp=0;tmp<12;tmp++)
				name[tmp] = '\0';
			for(i=1,j=0;i<8;i++){
				if(buf[i]!=32){
					name[j] = buf[i];
					j++;
				}
			}
			name[j++] = '.';
			for(;i<11;i++){
				if(buf[i]==32)
					break;
				name[j] = buf[i];
				j++;
			}
		}
	}else{ //if input is a dir
		for(i=0,j=0;i<11;i++){
			if(buf[i]!=32){
				name[j] = buf[i];
				j++;
			}
		}
	}
}

//transform the lowercase to uppercase
char* upperCase(char *str){
	int i=0;
	for(;i<strlen(str);i++){
		if(str[i]>=97&&str[i]<=122)
			str[i]-=32;
	}
	return str;
}

//milestone2, read out info in boot sector
void bootSector(char *argv[]){	
	FILE *fp = fopen(argv[2], "r");
	char buf[512];
	struct BootEntry *be;

	if(fp==NULL){
		fprintf(stderr, "Cannot find the file.\n");
		exit(0);
	}
	
	fread(buf, sizeof(buf), 1, fp);
	be = (struct BootEntry *)buf;

	fprintf(stdout, "Number of FATs = %u.\n", be->BPB_NumFATs);
	fprintf(stdout, "Numeber of bytes per sector = %u.\n", be->BPB_BytsPerSec);
	fprintf(stdout, "Numeber of sectors per cluster = %u.\n", be->BPB_SecPerClus);
	fprintf(stdout, "Numeber of reserved sectors = %u.\n", be->BPB_RsvdSecCnt);
	fclose(fp);
}

//milestone3, list files and sub-directories and files inside sub-directories
void listDir(char *argv[]){
	struct BootEntry *be = readBoot(argv);
	FILE *fp = fopen(argv[2], "r");
	int i=0; //output index
	long Fsize;
	char buf[32],bufTmp[512],Fname[16],*slash="/";
	struct DirEntry *de;
	unsigned long skipNum = 0,keepStart = 0; //start reading root directory position
	unsigned short startClustor = 0;

	if(fp==NULL){
		fprintf(stderr, "Cannot find the file.\n");
		exit(0);
	}

	skipNum = GetFsector(argv);
	keepStart = skipNum;
	// printf("===============skipNum:%ld\n",skipNum);

	fseek(fp, skipNum, 0);//right now the pointer's position is at the root directory
	fread(buf, sizeof(buf), 1, fp);
	while(buf[0]!=0){ //buf[0] will not be 0 except there is nothing in the field
		if(buf[0]==-27){
			skipNum+=32;
			fseek(fp, skipNum, 0);
			fread(buf, sizeof(buf), 1, fp);
		}
		/*if(buf[0]>=65&&buf[0]<=79){ //ignore long file name
			unsigned long tmpSkip = skipNum+32;
			char longTmp[512];
			fseek(fp, tmpSkip, 0);
			fread(buf, sizeof(buf), 1, fp);
			while(buf[0]>=1&&buf[0]<=15){ //0x01 to 0x0f
				tmpSkip+=32;
				fseek(fp, tmpSkip, 0);
				fread(buf, sizeof(buf), 1, fp);
			}
			tmpSkip+=32; //pass the longfile name's normal entry
			fseek(fp, tmpSkip, 0); 
			fread(buf, sizeof(buf), 1, fp); //a new file 
		}*/
			if(buf[0]!=0x00&&buf[0]!=-27&&buf[11]!=0x0f){
			// fread(buf, sizeof(buf), 1, fp); // go to the NORMAL ENTRY
			de = (struct DirEntry *)buf; //read the NORMAL ENTRY
			if((int)(de->DIR_FileSize)>0){ //if the checked entry is file's, print out the info
				startClustor = de->DIR_FstClusHI*100+de->DIR_FstClusLO;
			i++;
			trim(buf,(int)(de->DIR_FileSize));
			fprintf(stdout, "%d, %s, %u, %d\n", i,name,de->DIR_FileSize,startClustor);
			}else if(((int)(de->DIR_FileSize)==0)){ // if the checked entry is dir's, go inside it
				//printf("name ,,,,DIR_FileSize%s,,,,%d\n",de->DIR_Name,de->DIR_FileSize );
			char parentDir[12];
			int c;
			
			startClustor = de->DIR_FstClusHI*100+de->DIR_FstClusLO;
			i++;
			trim(buf,(int)(de->DIR_FileSize));
			
				//current dir
			if(startClustor!=0){
				fprintf(stdout, "%d, %s/ ,%u ,%d\n", i,name,de->DIR_FileSize,startClustor);
				i++;
				fprintf(stdout, "%d, %s/. ,%u ,%d\n", i,name,de->DIR_FileSize,startClustor);
				i++;
					//parent dir
				fprintf(stdout, "%d, %s/.. ,%u ,%d\n", i,name,0,0);
			}

				for(c=0;c<12;c++) //init
					parentDir[c] = name[c];

				fseek(fp, ((startClustor-2)*be->BPB_BytsPerSec+keepStart),0); //go inside the sub-dir
				//printf("===1111==============:%ld\n",((startClustor-2)*be->BPB_BytsPerSec+keepStart)),
				fread(buf, sizeof(buf), 1, fp);
				de = (struct DirEntry *)buf;
				while((de->DIR_Name[0])!=0){
					if(buf[0]!=-27){
						startClustor = de->DIR_FstClusHI*100+de->DIR_FstClusLO;
						i++;
						trim(buf,(int)de->DIR_FileSize);
						fprintf(stdout, "%d, %s/%s ,%u ,%d\n", i,parentDir,name,de->DIR_FileSize,startClustor);
						fread(buf, sizeof(buf), 1, fp);
						de = (struct DirEntry *)buf;
					}else{
						fread(buf, sizeof(buf), 1, fp);
						de = (struct DirEntry *)buf;
					}
				}
			}
		}
		skipNum = skipNum+32;
		fseek(fp, skipNum, 0);
		fread(buf, sizeof(buf), 1, fp);
		de = (struct DirEntry *)buf;
	}
	fclose(fp);
}

//go into the root directory to check the file exits or not
struct DirEntry* findFile(char *argv[]){
	struct BootEntry *be = readBoot(argv);
	FILE *fp = fopen(argv[2],"r");
	int w = 0, flag = -1,tmp;
	char buf[32],bufTmp[512],Fname[16],*filename=argv[4],*slash="/";
	struct DirEntry *de;
	unsigned long skipNum = GetFsector(argv); //start reading root directory position
	unsigned short startClustor = 0;

	fseek(fp, skipNum, 0);
	fread(buf, sizeof(buf), 1, fp);
	de = (struct DirEntry *)buf;
	for(rowNum=0;buf[0]!=0;rowNum++){ //search to the end of root directory
		// printf("---rowNum:%d-----buf[0]:%d-------\n",rowNum,buf[0]);
		if(buf[0]==-27){ //if find a e5 started file, read 32 bytes more
			//fread(buf, sizeof(buf), 1, fp); // go to the NORMAL ENTRY
			//de = (struct DirEntry *)buf; //read the NORMAL ENTRY
			
			filename=upperCase(argv[4]);
			// printf("-------------------filename:%s------\n",filename);
			// printf("------DDDDname:%s------\n",de->DIR_Name);

			trim(buf,(int)(de->DIR_FileSize));
			// printf("-------filename:%s---len:%d------name:%s-----len:%d------\n", filename,strlen(filename),name,strlen(name));

			for(w=1;w<=strlen(name);w++){
				// printf("------fn[%d]:%c----name[%d]:%c----flag:%d-----\n",w,filename[w],w-1,name[w-1],flag);
				if(strlen(filename)==(strlen(name)+1)){
					if(filename[w]!=name[w-1]){
						flag=-1;
						break; 
					}else{
						
						flag = 0;
					  // printf("------fn[%d]:%c----name[%d]:%c----flag:%d-----\n",w,filename[w],w-1,name[w-1],flag);
					}										  
				}else{
					flag = -1;
					break;
				}
			}
		 // printf("------flag:%d-------\n",flag);
			if(flag!=-1){//find under root
				//fprintf(stdout, "[file]----- in dir\n");
				fclose(fp);
				dir[30]="/";
				return de;
			}
		}else{
			int c=0;
			for(;c<30;c++)
				dir[c]='\0';
			//fread(buf, sizeof(buf), 1, fp); // go to next 32 byte
			//de = (struct DirEntry *)buf; // read into a struct
			if((int)de->DIR_FileSize==0){ //if next 32 byte is a dir's, go inside to find argv[4]
				//printf("de->DIR_FstClusHI*100+de->DIR_FstClusLO; %ld,,, %ld\n", de->DIR_FstClusHI,de->DIR_FstClusLO);
				// printf("filename:::%s\n", de->DIR_Name);
				startClustor = de->DIR_FstClusHI*100+de->DIR_FstClusLO;
			tmpStartClustor = (startClustor-2)*be->BPB_BytsPerSec+skipNum;

			trim(buf,(int)de->DIR_FileSize); 
			dir[30]="/";
			strcat(dir,slash);
			strcat(dir,name);
				fseek(fp, ((startClustor-2)*be->BPB_BytsPerSec+skipNum),0); //go inside the sub-dir
				fread(buf, sizeof(buf), 1, fp);
				de = (struct DirEntry *)buf;

				for(subRowNum=0;((de->DIR_Name[0])!=0);subRowNum++){// while((de->DIR_Name[1])!=0){
					if(buf[0]==-27){ //read 32 bytes more
						/*fread(buf, sizeof(buf), 1, fp);
						de = (struct DirEntry *)buf;*/

						filename=upperCase(argv[4]);
						trim(buf,(int)(de->DIR_FileSize));
						//printf("=====name:%s\n",name);
						for(w=1;w<=strlen(name);w++){
							//printf("------fn[%d]:%c----name[%d]:%c----flag:%d-----\n",w,filename[w],w-1,name[w-1],flag);
							if(strlen(filename)==(strlen(name)+1)){
								if(filename[w]!=name[w-1]){
									flag=-1;
									break; 
								}else{				
									flag = 0;
					  				// printf("------fn[%d]:%c----name[%d]:%c----flag:%d-----\n",w,filename[w],w-1,name[w-1],flag);
								}
							}else{
								flag = -1;
								break;
							}
						}	
						if(flag!=-1){//find under root
							rowNum = -1;
							// fprintf(stdout, "[file]----- in dir\n");
							fclose(fp);
							return de;
						}
					}	
					fread(buf, sizeof(buf), 1, fp);
					de = (struct DirEntry *)buf;
					//printf("ssssssssssssss=:%s\n",de->DIR_Name );
				}
			}
		}	
		skipNum = skipNum+32;
		fseek(fp, skipNum, 0);
		fread(buf, sizeof(buf), 1, fp);
		de = (struct DirEntry *)buf;
	}
	fclose(fp);
	return NULL;
}

void dToO(int dNum,char* oNum){
	char temp[8]={0};
	int i;
	for (i=0;(dNum)&&(i<8);i++){
		temp[i]=(dNum%16);
		if(temp[i]>9){
			temp[i]=oNumTable[temp[i]-10];
		}
		else{
			temp[i]+='0';
		}
		dNum/=16;
	}
	char* p=temp;

	for(i=7;*p;i--,p++){
		oNum[i]=*p;
	}
}

void try_recover(unsigned short position,unsigned long fileSize,char *argv[]){
	struct BootEntry *be = readBoot(argv);
	
	// printf("%d\n",rowNum );
	FILE *fp = fopen(argv[2],"rw+");
	long rootposition=tmpStartClustor;
	if(rowNum==-1){

		//printf("subRowNum=======%d\n", subRowNum);
		rootposition=rootposition+subRowNum*32;
		//printf("%d\n", rootposition);
	}
	else{
		rootposition=GetFsector(argv);
	//printf("rowNum=======%d\n", rowNum);
		rootposition+=rowNum*32;

	}

	// long rootposition=GetFsector(argv);
	// // printf("================bp:%ld\n",rootposition );
	// rootposition+=rowNum*32;
	// // printf("%d\n",rootposition );


	if(fp==NULL)
		printf("can not open the file!\n");
	int i = 0, flag = 1,tmp;
	int s;
	char bufTmp[512],Fname[16],*filename=argv[4];
	unsigned long skipNum = GetFatsector(argv); //start reading fat position
	long clusterNum;
	// printf("%d,%d",position,fileSize);
	if(fileSize==0){
		clusterNum=1;
	}
	else if(fileSize%be->BPB_BytsPerSec>0){
		clusterNum=fileSize/be->BPB_BytsPerSec+1;
	}
	else{
		clusterNum=fileSize/be->BPB_BytsPerSec;
	}
	char buf[clusterNum*4];
	skipNum=skipNum+position*4;
	fseek(fp, skipNum, 0);
	fread(buf, sizeof(buf), 1, fp);
	while(i< sizeof(buf)){
		if(buf[i]!=0){
			flag=0;
			break;
		}
		i++;
	}
	if(flag){
		//skip to the right position of the FAT table 
		fseek(fp, skipNum, 0);
		//each time write 4 bytes 
		for(i=0;i<clusterNum-1;i++){

			char oNum[8]="00000000";
			char fpart[3]="00";
			char spart[3]="00";
			char tpart[3]="00";
			char lpart[3]="00";

			//find the next position in the FAT table
			position++;


			
			dToO(position,oNum);
			
			fpart[0]=oNum[6];
			fpart[1]=oNum[7];
			spart[0]=oNum[4];
			spart[1]=oNum[5];
			tpart[0]=oNum[2];
			tpart[1]=oNum[3];
			lpart[0]=oNum[0];
			lpart[1]=oNum[1];
			fpart[2]='\0';
			spart[2]='\0';
			tpart[2]='\0';
			lpart[2]='\0';
			//printf("%s\n",oNum);
			//printf("%s,%s,%s,%s\n",fpart,spart,tpart,lpart);
			unsigned char f[2];
			unsigned char s[2];
			unsigned char t[2];
			unsigned char l[2];
			sscanf(fpart,"%2x",f);
			sscanf(spart,"%2x",s);
			sscanf(tpart,"%2x",t);
			sscanf(lpart,"%2x",l);
			//printf("%s,",oNum);
			fwrite(f,1,1,fp);
			fwrite(s,1,1,fp);
			fwrite(t,1,1,fp);
			fwrite(l,1,1,fp);

		//	fwrite(position,4,1,fp);
		}
		
		fwrite("\xff\xff\xff\x0f",4,1,fp);

		
		fseek(fp,rootposition,0);
		char *ptrArgv = argv[4];
	//printf("%c\n",ptrArgv[0] );
		char reconame=ptrArgv[0] ;

		fwrite(&reconame,1,1,fp);
		printf("[%s]:\trecoverd in [%s]\n",argv[4],dir);

	}
	else{
		printf("error - fail to recover\n");
	}

}

//milestone4,5,6
void recover(char *argv[]){
	struct DirEntry *de;

	if((de=findFile(argv))!=NULL){
	 	//printf("---start cluster:%d, filesize:%ld-------\n",de->DIR_FstClusLO+de->DIR_FstClusHI,de->DIR_FileSize);
		unsigned long de_size=de->DIR_FileSize; 
		unsigned short de_fc=de->DIR_FstClusLO+de->DIR_FstClusHI;
	 	// printf("desize:=====%ld\n",de_size);
	 	//try_recover(de->DIR_FstClusLO+de->DIR_FstClusHI,de->DIR_FileSize,argv);
		try_recover(de_fc,de_size,argv);
	} 	
	else
		printf("[%s]:\terror - file not found\n",argv[4]);

}

int main(char argc, char *argv[]){
	unsigned long skipNum;

	//printf("argc is: %d\t",argc);
	//printf("argv[argc-1] is %s\n\n",argv[argc-1]);

	//input is invalid
	if(validArg(argc,argv)==-1)
		return -1;
	//input is valid
	else{
		if(strcmp(argv[3],"-i")==0){
			bootSector(argv);	
		}else if(strcmp(argv[3],"-l")==0){
			listDir(argv);
		}else{
			recover(argv);
		}
	}

	return 0;
}
