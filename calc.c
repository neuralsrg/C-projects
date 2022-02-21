#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int INPUT_LENGTH = 100;
const int STACK_LENGTH = 100;
const int STRING_WIDTH = 30;

/* functions */

void push(char item[], int *top, char s[][STRING_WIDTH]);
char *pop(int *top, char s[][STRING_WIDTH]);
void initstack(int *top, char s[][STRING_WIDTH]);
void getnumber(int *index, char s[INPUT_LENGTH], char (*num)[STRING_WIDTH]);
void compute_result(int *top, char s[][STRING_WIDTH], char sign);
void switch_notation(char number[STRING_WIDTH], int in_notation, int out_notation, char (*output)[STRING_WIDTH]);

int main() {
	
	/* Hyperparameters */

	int in_notation = 10, out_notation = 10;
	int out_accuracy = 6;	

	int i = 0, accuracy_counter = 0;
	int unary = 1; /* is it possible to get unary?*/

	/* Stacks */

	char s[STACK_LENGTH][STRING_WIDTH]; 
	int top = -1;

	char polskiy_stack[STACK_LENGTH][STRING_WIDTH];
	int polskiy_top = -1;

	/* Stacks initialization */

	initstack(&top, s);
	initstack(&polskiy_top, polskiy_stack);

	char number[STRING_WIDTH], decimal_number[STRING_WIDTH], *sign;
	
	/* Computation variables */

	//char *op1, *op2;
	double result;
	//char res_char[STRING_WIDTH];

	char symb[2];
	symb[1] = 0;

	char input_data[INPUT_LENGTH + 1];
	int ilen;

	/* Notations */
	
	printf("\nEnter the input notation [2, 36]: ");
	gets(input_data);
	
	if(atoi(input_data) != 0) {
		in_notation = atoi(input_data);	
	}

	printf("\nEnter the output notation [2, 36]: ");
	gets(input_data);

	if(atoi(input_data) != 0) {
		out_notation = atoi(input_data);	
	}

	printf("\nEnter the accuracy of output: ");
	gets(input_data);

	if(atoi(input_data) != 0) {
		out_accuracy = atoi(input_data);	
	}

	/* Reading input */

	printf("\nEnter data: \n");
	gets(input_data);
	ilen = strlen(input_data);
	//printf("Got data: %s\n", input_data);
	//printf("Strlen = %d\n", strlen(input_data));

	symb[0] = '(';
	push(symb, &top, s);
	input_data[strlen(input_data)] = ')';
	input_data[strlen(input_data) + 1] = 0;
	
	//printf("Last strlen was %d\n", strlen(input_data));
	for(i = 0; i <= ilen; i++) {

		if(input_data[i] == ' ') {
			continue;
		}  

		if(isalnum(input_data[i])) {
			//printf("i = %d\n", i);
			getnumber(&i, input_data, &number);
			//printf("Calling SW with number = %s\n", number);
			switch_notation(number, in_notation, 10, &decimal_number);
			//printf("pushing %s\n", decimal_number);
			push(decimal_number, &polskiy_top, polskiy_stack);
			unary = 0;
		} 

		switch(input_data[i]) {

			case '(':
				symb[0] = input_data[i];
				push(symb, &top, s);
				unary = 1;
				break;

			case ')':
				while(top > -1) {
					sign = pop(&top, s);
					if(sign[0] == '(') {
						break;
					}
					push(sign, &polskiy_top, polskiy_stack);
					unary = 0;
				}
				break;

			case '+':
			case '-':
				if(unary) {
					symb[0] = '0';
					push(symb, &polskiy_top, polskiy_stack);
				}
				if(top != -1) {
					do {
						sign = pop(&top, s);
						if(sign[0] == '(') {
							push(sign, &top, s);
							break;
						}
						push(sign, &polskiy_top, polskiy_stack);
					} while(top != -1);
				}
				symb[0] = input_data[i];
				push(symb, &top, s);
				unary = 0;
				break;

			case '/':
			case '*':
				if(top != -1) {
					do {
						sign = pop(&top, s);
						if((sign[0] == '(') || (sign[0] == '+') || (sign[0] == '-')) {
							push(sign, &top, s);
							break;
						}
						push(sign, &polskiy_top, polskiy_stack);
					} while(top != -1);
				}
				symb[0] = input_data[i];
				push(symb, &top, s);
				unary = 0;
		}
	}
	/*
	printf("\npolskiy_stack\n");
	for(i = 0; i < STACK_LENGTH; i++) {
		printf("%s\n", polskiy_stack[i]);
	}*/	
	
	initstack(&top, s);

	for(i = 0; i < STACK_LENGTH; i++) {
		 
		if(polskiy_stack[i][0] == 0) {
			break;
		}

		if(isalnum(polskiy_stack[i][0])) {
			push(polskiy_stack[i], &top, s);	
		}
		
		switch(polskiy_stack[i][0]) {

			case '+':
				compute_result(&top, s, '+');
				break;
			case '-':
				compute_result(&top, s, '-');
				break;
			case '*':
				compute_result(&top, s, '*');
				break;
			case '/':
				compute_result(&top, s, '/');
				break;
		}	
	}
	result = atof(pop(&top, s));
	if(result >= 0) {
		snprintf(decimal_number, STRING_WIDTH, "%.15f", result);
	} else {
		printf("-");
		result = -result;
		snprintf(decimal_number, STRING_WIDTH, "%.15f", result);
	}
	switch_notation(decimal_number, 10, out_notation, &number);
	//switch_notation(pop(&top, s), 10, out_notation, &number);
	i = 0;
	accuracy_counter = 0;
	while(1) {
		if(number[i] == '.') {
			accuracy_counter = 1;
		}
		printf("%c", number[i]);
		if(accuracy_counter) {
			if(accuracy_counter++ == out_accuracy + 1) {
				break;
			}
		}
		i++;
	}
	printf("\n");
	//printf("Result: %.*f", 10, result);
	return 0;
}


void push(char item[], int *top, char s[][STRING_WIDTH])
{
    strcpy(s[++*top], item);
}


char *pop(int *top, char s[][STRING_WIDTH])
{
    char *item;
    item = s[(*top)--];
    return item;
}


void initstack(int *top, char s[][STRING_WIDTH]) {

	int i;
	char zero[1];
	zero[0] = 0;

	*top = -1;
	
	for(i = 0; i < STACK_LENGTH; i++) {
		push(zero, top, s);	
	}

	*top = -1;
}


void getnumber(int *index, char s[INPUT_LENGTH], char (*num)[STRING_WIDTH]) {
	int i = 0, j;
	while(1){
		if((isalnum(s[*index])) || (s[*index] == '.')) {
			(*num)[i++] = s[*index];
			(*index)++;
		} else {
			for(j = i; j < strlen(num); j++) {
				(*num)[j] = 0;
			}
			break;
		}
	}
}


void switch_notation(char number[STRING_WIDTH], int in_notation, int out_notation, char (*output)[STRING_WIDTH]) {

	double decimal = 0;
	int int_part, index = 0;
	double period_part;
	int digit, i = 0, frac = 0, counter = in_notation;
	char reversed_output[STRING_WIDTH];
	char symb;

	//printf("[SW] Called with number %s\n", number);
	if(in_notation != 10) {
		
		for(i = 0; i < strlen(number); i++) {
			if(number[i] == '.') {
				frac = 1;
				continue;
			}

			if(isalpha(number[i])) {
				digit = toupper(number[i]);
				digit = digit - 'A' + 10;
			} else {
				digit = number[i] - '0';
			}

			if(!frac) {
				decimal *= in_notation;
				decimal += digit; 
			} else {
				decimal += digit * (1.0 / counter);
				counter *= in_notation;
			}
		}	
	} else {
		decimal = atof(number);
	}	

	//printf("decimal = %f\n", decimal);
	i = 0;

	if(out_notation != 10) {
		int_part = decimal;
		period_part = decimal - int_part;

		while(int_part > 0) {
			
			if(int_part % out_notation <= 9) {
				symb = '0' + int_part % out_notation;
			} else {
				symb = 'A' + int_part % out_notation - 10;
			}
			
			int_part /= out_notation;
			reversed_output[i++] = symb;
		}

		reversed_output[i] = 0;
		(*output)[i] = 0;
		//printf("Reversed output: %s\n", reversed_output);

		for(i = 0; i<strlen(reversed_output); i++) {
			(*output)[i] = reversed_output[strlen(reversed_output)-1-i];
			//printf("Current index: %d\n", strlen(reversed_output)-1-i);
		}
		
		if(i == 0) {
			(*output)[i++] = '0';
		}
		(*output)[i++] = '.';
		counter = 0;

		//printf("period_part = %f\n", period_part);

		//while((period_part != 0) && (counter < 15)) {
		while(counter < 15) {
			period_part *= out_notation;
			int_part = period_part;
			period_part -= int_part;
			//printf("int_part = %d\n", int_part);
			
			if(int_part <= 9) {
				symb = '0' + int_part;
			} else {
				symb = 'A' + int_part - 10;
			}
			(*output)[i++] = symb;
			counter++;
		}
		(*output)[i] = 0;
	} else {
		snprintf(*output, STRING_WIDTH, "%.15f", decimal);
	}
}


void compute_result(int *top, char s[][STRING_WIDTH], char sign) {

	char *op1, *op2;
	double result;
	char res_char[STRING_WIDTH];
	
	op2 = pop(top, s);
	op1 = pop(top, s);
	
	switch(sign) {
		
		case '+':
			result = atof(op1) + atof(op2);
			snprintf(res_char, STRING_WIDTH, "%.15f", result);
			push(res_char, top, s);
			//printf("[+] computed %s\n", res_char);
			break;
		case '-':
			result = atof(op1) - atof(op2);
			snprintf(res_char, STRING_WIDTH, "%.15f", result);
			push(res_char, top, s);
			//printf("[-] computed %s\n", res_char);
			break;
		case '*':
			result = atof(op1) * atof(op2);
			snprintf(res_char, STRING_WIDTH, "%.15f", result);
			push(res_char, top, s);
			//printf("[*] computed %s\n", res_char);
			break;
		case '/':
			result = atof(op1) / atof(op2);
			snprintf(res_char, STRING_WIDTH, "%.15f", result);
			push(res_char, top, s);
			//printf("[*] computed %s\n", res_char);
			break;
	}
}
