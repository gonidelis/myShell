#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <errno.h>


void takeIn(char in[]){

	fgets(in,512,stdin);
	
	//exclude '\n' character (enter) from string-input
	int i = 0;
	while (in[i]!='\0'){
		i++;
	}
	in[i-1]='\0';
}


void parse(char input[], char* toks[],const char delimeter[]){
	//general parsing function
	char *token;
	int i=0;
	token = strtok(input, delimeter);

	while( token != NULL ) {

		toks[i]=token;
     	i ++;
      	token = strtok(NULL, delimeter);

  	}
  	toks[i] = NULL;

}


int seperateCommands(char input[],char *cmds[], bool* success){
	char *token; 	
	int i=0,pos=0;
	
	//find all delimiters
	for(int j=0;j<strlen(input)-1;j++){
		if(input[j]=='&' && input[j+1]=='&'){
			success[pos]=true;
			pos++;
		}else if(input[j]==';'){
			success[pos]=false;
			pos++;
		}
	}

	//split the commands

	parse(input, cmds, ";&&"); //parse iput in ';' or '&&'

  	return pos+1; 	//pos+1 = number of commands
}


int executeCmd(char* tokens[]){
	int status;
	pid_t child_pid;

	child_pid = fork();

  	if(child_pid == 0) {
    	/*child process. */
    	if (execvp(tokens[0], tokens)<0){
    		perror(tokens[0]); 
    		return -1;
    	}
 	}else if(child_pid<0){
 		perror("Fork failed");
 		return 1;
	} else {
        waitpid(child_pid, &status, WUNTRACED);
    }
	if (status==0){
		return 0;
	}else{
		return -1;
	}
}		

int findPipe(char cmds[]){
	if(strchr(cmds, '|')!=NULL){
		return 1;
	}else{
		return 0;
	}
}

int findRedirection(char cmds[]){

	if(strchr(cmds, '>')!=NULL){
		return 1;
	}else if(strchr(cmds, '<')!=NULL){
		return 2;
	}else{
		return 0;
	}
}

int executeRedirection(char cmds[], int f){

	char *tokens1[512];
	char *tokens2[512];
	int status;
	pid_t child_pid;

	parse(cmds, tokens1, "<>"); //parse in spaces or '<' or '>'

	parse(tokens1[0], tokens2, " \t");

	char *token= strtok(tokens1[1], " \t");
	
	while( tokens1[1] == " \t") {

      	token = strtok(NULL, " \t");

  	}

	char *fileName= token;

	FILE* file = fopen(fileName, "a+");
	 
	child_pid = fork();

  	if(child_pid == 0) {
    	/* This is done by the child process. */
    	if(f==1){
    		close(1); //close stdout
			dup2(fileno(file),fileno(stdout)); //stout goes to file        	

		}else if(f==2){
			close(0); //close stdin
			dup2(fileno(file),fileno(stdin)); //stdin is in file
		}

    	if (execvp(tokens2[0], tokens2)<0){
    		perror(tokens2[0]); 
    		exit(1);
    	}
 	}else if(child_pid<0){
 		perror("Fork failed");
 		dup2(fileno(file),fileno(stderr)); //stderr goes to file
 		exit(1);
	} else {
		fclose(file);
        waitpid(child_pid, &status, WUNTRACED);
    }

	if (status==0){
		return 0;
	}else{
		return -1;
	}

}



int executePipe(char cmds[]){
	int stat1, stat2;	//process1 status1,process2 status2
	pid_t pid1, pid2;
	int fd[2];
	char *tokens1[512];
	char *tokens2[512];
	char *tokens3[512];

	parse(cmds, tokens1, "|"); //tokens1 contains the cmds 

	// Create a pipe.
	if(pipe(fd) == -1){
		perror("Pipe failed");
        return -1;
	}

	int r1,r2;
	r1=findRedirection(tokens1[0]);
	r2=findRedirection(tokens1[1]);

	// Create our first process.
	pid1 = fork();
	if (pid1 < 0) {
      perror("Fork failed");
      exit(1);
    }else if (pid1 == 0) { 
        dup2(fd[1], 1);
        close(fd[0]);
        if(r1==0){
        	parse(tokens1[0], tokens2, " \t"); //tokens2 the 1st cmd(pefore pipe)
        	execvp(tokens2[0], tokens2); // run command BEFORE pipe character in userinput
        	perror("Execvp failed");
        	exit(1);
        }else{
        	executeRedirection(tokens1[0],r1);
        	exit(0);
        }
        
    } 
    close(fd[1]);
	// Wait for everything to finish and exit.
	waitpid(pid1, &stat1, WUNTRACED);

	// Create our second process.
    pid2 = fork();
    if (pid2 < 0) {
      perror("Fork failed");
      exit(1);
    }else if (pid2 == 0) { 
        dup2(fd[0], 0);
        close(fd[1]);
        if(r2==0){
        	parse(tokens1[1], tokens3, " \t"); //tokens3 the 2nd cmd(after pipe)
        	execvp(tokens3[0], tokens3); // run command AFTER pipe character in userinput
        	perror("Execvp failed");
        	exit(1);
        }else{
        	executeRedirection(tokens1[1],r2);
        	exit(0);
        }
    }
    close(fd[0]);
    waitpid(pid2, &stat2, WUNTRACED);


    if (stat1==0 && stat2==0){
    	return 0;
    }else{
    	return -1;
    }
    

}

void parseExec(char *input){

	int redirectionFound=-10;	//random non valid initialization
	int pipeFound=-10;
	int numOfCmds=0;
	char *cmds[512]; 	//commands stored seperately
	char *toks[512]; 	//parts of each command stored seperately
	bool *success=(bool *)malloc(512*sizeof(bool));
	//**********parse multiple commands**************
	numOfCmds = seperateCommands(input, cmds, success);
	//***********parse/execute each command**************************
	for (int i =0 ; i<numOfCmds; i++){
		//check for exit
		if(strcmp(cmds[i],"quit") == 0){
			exit(0);
		}

		pipeFound=findPipe(cmds[i]);
		redirectionFound=findRedirection(cmds[i]);
		
		if(pipeFound==0 && redirectionFound==0){
			//Neither pipe nor redirection found
			parse(cmds[i], toks, " \t"); //parse space
			if (executeCmd(toks)==-1 && success[i]==1){
				break;
			}
		
		}else if(pipeFound==0 && (redirectionFound==1 || redirectionFound==2)){
			//Redirection only
			if (executeRedirection(cmds[i],redirectionFound)==-1 && success[i]==1){
				break;
			}
		}else{
			//pipe only
			if (executePipe(cmds[i])==-1 && success[i]==1){
				break;
			}
		}
	}

}


void shell(){
	//user_aem
	printf("gonidelis_8794>");

	//*************initiallization*****************
	char in[512]; 	//string-input from user
	
	//***********take input from user****************
	takeIn(in);
	parseExec(in);
	shell();
}

int batchfile_mode(char * argv[]){

	char str[512];
	char *batch_cmds[512];
	int i=0, j=0; 
	int n;	//number of batchfile commands

	FILE *fp;
	fp = fopen(argv[1], "r");

	if(fp == NULL) {
  		perror("Error opening file");
  		return(-1);
	 	}
	 	
	while(fgets(str,512, fp)!=NULL){
		batch_cmds[i] = strdup(str);
		i++;
		n = i;
	}

	for (i=0;i<n;i++){

		
		batch_cmds[i] = strtok(batch_cmds[i], "\n");

		parseExec(batch_cmds[i]);
	}
	

	fclose(fp);

	return 0;
}


int main(int argc, char *argv[]){
	if (argc >=2){
		//batchfile mode
		batchfile_mode(argv);

	}else{
		//interactive mode
		shell();	//shell() called recursively	
	}
	
	return 0;
}