#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "datatypes.h"

#define BUFFER_SIZE 255

unsigned int TAB_WIDTH = 4;

static bool is_space(char c) {
    return c == '\t' || c == ' ';
}

static bool is_newline(char c) {
    return c == '\n' || c == '\r';
}

void begin_atom(parse * p) {
    p->buffer_index = 0;
    p->state = COLLECTING_ATOM;
}

swexp_list_node * close_atom(parse * p) {
    swexp_list_node * node = malloc(sizeof(swexp_list_node));
    char * content = malloc(p->buffer_index + 1);
    memcpy(content, p->buffer, sizeof(char) * p->buffer_index);
    content[p->buffer_index + 1] = '\0';

    node->content = content;
    node->type = ATOM;

    p->state = SKIP_SPACE;
    return node;
}

void build_atom(parse * p, char c) {
    if (p->buffer_index > BUFFER_SIZE) {
        printf("buffer overflow \"%*s\"", BUFFER_SIZE, p->buffer);
    }
    p->buffer[p->buffer_index] = c;
    p->buffer_index++;
}

unsigned int list_len(swexp_list_node * node) {
    unsigned int count = 0;
    while(node != NULL){
        node = node->next;
        count ++;
    }
    return count;
}


swexp_list_node * chain_tail(swexp_list_node * list) {
    for(; list->next != NULL; list = list->next){}
    return list;
}

swexp_list_node * list_tail(swexp_list_node * list) {
    if (list->type != LIST) {
        printf("tried to get tail of non list");
        exit(1);
    }

    return chain_tail((swexp_list_node *) list->content);
}

swexp_list_node * parse_line(parse * p) {
    // parses a line of text, starting at a non-whitespace char
    char c;

    // build a list of expressions started by this
    // list head on the stack.
    swexp_list_node head, *tail;
    head.next = NULL;
    head.content = NULL;
    head.type = UNDEFINED;
    tail = &head;

    p->state = SKIP_SPACE;

    while((c = fgetc(p->f)) != EOF && !is_newline(c)) {
        switch(p->state) {
            case COLLECTING_ATOM:
                if (is_space(c)) {
                    // end atom
                    tail->next = close_atom(p);
                    tail = tail->next;
                    fseek(p->f, -1, SEEK_CUR);
                } else {
                    // continue to build item
                    build_atom(p, c); 
                }
                break;
            case SKIP_SPACE:
                if (!is_space(c)) {
                    begin_atom(p);
                    fseek(p->f, -1, SEEK_CUR);
                }
                break;
            default:
                printf("unexpected state in parse_line\n");
                exit(1);
        } 
    }

    if(is_newline(c)){
        p->indentation = 0;
    }

    // close ongoing capture
    if(p->state == COLLECTING_ATOM) {
        tail->next = close_atom(p);
    }
    
    // reset state
    p->state = COUNTING_INDENTATION;

    // if the number of collected atoms is more than one,
    // make it a list and return it
    if(list_len(head.next) > 1) {
        swexp_list_node * listhead = malloc(sizeof(swexp_list_node));
        listhead->type = LIST;
        listhead->next = NULL;
        listhead->content = head.next;
        return listhead;
    } else {
        return head.next;
    }
}

swexp_list_node * parse_block(parse * p) {
    // parses a block of lines with the same indentation into a chain
    // (not a list)
    char c;

    swexp_list_node fakehead, *tail;
    fakehead.next = NULL;
    fakehead.content = NULL;
    fakehead.type = UNDEFINED;
    tail = &fakehead;

    // get initial indentation by consuming characters until we find some
    unsigned int current_indentation;
    for (current_indentation = p->indentation;
            is_space(fgetc(p->f));
            current_indentation++){}
    fseek(p->f, -1, SEEK_CUR);

    while((c = fgetc(p->f)) != EOF) {
        switch(p->state) {
            case COUNTING_INDENTATION:
                if (is_space(c)) {p->indentation++;}
                else if (is_newline(c)){p->indentation = 0;}
                else {
                    // this is a start of an atom.
                    // parse as appropriate based on indent
                    fseek(p->f, -1, SEEK_CUR);
                    if (p->indentation > current_indentation) {
                        if (tail->type == ATOM) {
                            // make the tail a list before appending
                            swexp_list_node * tailcont = 
                                malloc(sizeof(swexp_list_node));
                            tailcont->type = ATOM;
                            tailcont->next = NULL;
                            tailcont->content = tail->content;

                            tail->type = LIST;
                            tail->content = tailcont;
                        }
                        // append it to the list that is the last
                        // element
                        list_tail(tail)->next = parse_block(p);
                    } else if (p->indentation == current_indentation) {
                        tail->next = parse_line(p);
                        tail = tail->next;
                    } else {
                        goto clean_and_return;
                    }
                }
                break;
            default:
                printf("unexpected state in parse_line\n");
                exit(1);
        }
    }

    clean_and_return:
    return fakehead.next;
}

swexp_list_node * parse_file_to_atoms(FILE *f) {
    char buffer[BUFFER_SIZE];
    parse p = {
        f, COUNTING_INDENTATION, 
        (char*) &buffer, 0, 0};
    swexp_list_node * container = malloc(sizeof(swexp_list_node));
    container->type = LIST;
    container->next = NULL;
    container->content = parse_block(&p);
    return container;
}

void traverse_list(swexp_list_node * node,
        void (*callback)(swexp_list_node *)) {
    callback(node);
    if(node != NULL && node->type == LIST) {
        traverse_list(
                (swexp_list_node *) node->content,
                callback);
    }
    
    if (node != NULL) {
        traverse_list(node->next, callback);
    }
}
