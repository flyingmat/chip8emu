#pragma once
#include <cstdlib>
#include <cmath>

class Chip8Cpu {
private:
    unsigned char* v;
    unsigned short int i;
    unsigned short int pc;
    unsigned char sp;

    unsigned int frequency;
    double delay;

    void clear_screen(unsigned char*& memory) {
        for (size_t i = 0xF00; i <= 0xFFF; i++)
            memory[i] = 0;
    }

    bool draw_sprite(unsigned char*& memory, unsigned char x, unsigned char y, unsigned char h) {
        bool setvf = false;

        printf("  drawing: I = %04x @(%d, %d)\n", this->i, x, y);
        for (size_t row = 0; row < h; row++) {
            unsigned short int memptr = 0xF00 + ((y + row) * 8) + floor(x / 8);
            unsigned char newmem;
            unsigned char oldmem;

            if (!(x % 8)) {
                oldmem = memory[memptr];
                newmem = oldmem ^ memory[this->i + row];
                memory[memptr] = newmem;
            } else {
                oldmem = (memory[memptr] << (x % 8)) | (memory[memptr+1] >> (8 - (x % 8)));
                newmem = oldmem ^ memory[this->i + row];

                // all this stuff can be simplified todo
                // memory[memptr] = (memory[memptr] >> (8 - (x % 8))) << (8 - (x % 8));
                // memory[memptr] |= newmem >> (x % 8);
                // memory[memptr+1] = (memory[memptr+1] << (x % 8)) >> (x % 8);
                // memory[memptr+1] |= newmem << (8 - (x % 8));
                memory[memptr] ^= memory[this->i + row] >> (x % 8);
                memory[memptr+1] ^= memory[this->i + row] << (8 - (x % 8));
            }

            if ((oldmem & newmem) != 0x00)
                setvf = true;

            printf("  -> %04x: %02x (I=%04x)\n", memptr, (memory[this->i + row]), (this->i + row));
        }

        return setvf;
    }

    void print_stack(unsigned char*& memory) {
        for (size_t i = 0; i < this->sp; i++)
            printf("  %04x: %02x\n  %04x: %02x\n",
                (0xEA0 - 2 + (this->sp * 2)),
                memory[0xEA0 - 2 + (this->sp * 2)],
                (0xEA0 - 2 + (this->sp * 2) + 1),
                memory[0xEA0 - 2 + (this->sp * 2) + 1]
            );
    }
public:
    Chip8Cpu(unsigned int frequency) {
        // allocate registers
        this->v = new unsigned char [16];
        this->i = 0;
        this->pc = 0x200;
        this->sp = 0;

        // set cpu frequency
        this->frequency = frequency;
        this->delay = (1.0 / frequency) * 1000;

        // check for allocation errors
        if (this->v == NULL)
            throw std::runtime_error("Failed to allocate memory!");

        // erase registers (init)
        for (size_t i = 0; i < 16; i++)
            this->v[i] = 0;
    }

    void cycle(unsigned char*& memory, Chip8Screen*& screen, Chip8Timer*& delay_timer, Chip8Timer*& sound_timer) {
        unsigned short int instruction = (memory[this->pc] << 8) | (memory[this->pc + 1]);
        unsigned long int ticks = SDL_GetTicks();

        printf("frequency %d, delay %9.6f\n", this->frequency, this->delay);
        printf("@%04x: %04x - ", this->pc, instruction);
        printf("dt: %d, st: %d\n", delay_timer->get_value(), sound_timer->get_value());
        printf("I = %04x, V [ ", this->i);

        for (size_t i = 0; i <= 0xF; i++) {
            printf("%01X: %02x", i, this->v[i]);
            if (i != 0xF)
                printf(", ");
        }
        printf(" ]\n");

        if (instruction == 0xE0)
            this->clear_screen(memory);
        else if (instruction == 0xEE) {
            this->print_stack(memory);
            this->pc = memory[0xEA0 - 2 + (this->sp * 2)] << 8;
            this->pc ^= memory[0xEA0 - 2 + (this->sp-- * 2) + 1];
            printf("  returning to: %04x, sp=%d\n", this->pc, this->sp);
        }
        else if ((instruction >> 12) == 0x1)
            this->pc = (instruction & 0x0FFF) - 2;
        else if ((instruction >> 12) == 0x2) {
            memory[0xEA0 - 2 + (++this->sp * 2)] = this->pc >> 8;
            memory[0xEA0 - 2 + (this->sp * 2) + 1] = this->pc;
            this->print_stack(memory);
            this->pc = (instruction & 0x0FFF) - 2;
            printf("  calling fn @: %04x, sp=%d\n", this->pc+2, this->sp);
        }
        else if ((instruction >> 12) == 0x3) {
            if (this->v[(instruction >> 8) & 0x000F] == (instruction & 0x00FF))
                this->pc += 2;
        }
        else if ((instruction >> 12) == 0x4) {
            if (this->v[(instruction >> 8) & 0x000F] != (instruction & 0x00FF))
                this->pc += 2;
        }
        else if ((instruction >> 12) == 0x5) {
            if (this->v[(instruction >> 8) & 0x000F] == this->v[(instruction >> 4) & 0x000F])
                this->pc += 2;
        }
        else if ((instruction >> 12) == 0x6)
            this->v[(instruction >> 8) & 0x000F] = (instruction & 0x00FF);
        else if ((instruction >> 12) == 0x7)
            this->v[(instruction >> 8) & 0x000F] += (instruction & 0x00FF);
        else if ((instruction >> 12) == 0x8) {
            if ((instruction & 0x000F) == 0x0)
                this->v[(instruction >> 8) & 0x000F] = this->v[(instruction >> 4) & 0x000F];
            else if ((instruction & 0x000F) == 0x1)
                this->v[(instruction >> 8) & 0x000F] = this->v[(instruction >> 8) & 0x000F] | this->v[(instruction >> 4) & 0x000F];
            else if ((instruction & 0x000F) == 0x2)
                this->v[(instruction >> 8) & 0x000F] = this->v[(instruction >> 8) & 0x000F] & this->v[(instruction >> 4) & 0x000F];
            else if ((instruction & 0x000F) == 0x3)
                this->v[(instruction >> 8) & 0x000F] = this->v[(instruction >> 8) & 0x000F] ^ this->v[(instruction >> 4) & 0x000F];
            else if ((instruction & 0x000F) == 0x4) {
                if (this->v[(instruction >> 8) & 0x000F] + this->v[(instruction >> 4) & 0x000F] > 0xFF)
                    this->v[0xF] = 0x1;
                else
                    this->v[0xF] = 0x0;
                this->v[(instruction >> 8) & 0x000F] += this->v[(instruction >> 4) & 0x000F];
            }
            else if ((instruction & 0x000F) == 0x5) {
                if (this->v[(instruction >> 4) & 0x000F] <= this->v[(instruction >> 8) & 0x000F])
                    this->v[0xF] = 1;
                else
                    this->v[0xF] = 0;
                this->v[(instruction >> 8) & 0x000F] -= this->v[(instruction >> 4) & 0x000F];
            }
            else if ((instruction & 0x000F) == 0x6) {
                this->v[0xF] = (instruction & 0x0001);
                this->v[(instruction >> 8) & 0x000F] >>= 1;
            }
            else if ((instruction & 0x000F) == 0x7) {
                if (this->v[(instruction >> 4) & 0x000F] >= this->v[(instruction >> 8) & 0x000F])
                    this->v[0xF] = 1;
                else
                    this->v[0xF] = 0;
                this->v[(instruction >> 8) & 0x000F] = this->v[(instruction >> 4) & 0x000F] - this->v[(instruction >> 8) & 0x000F];
            }
            else if ((instruction & 0x000F) == 0xE) {
                this->v[0xF] = (instruction & 0x8000);
                this->v[(instruction >> 8) & 0x000F] <<= 1;
            }
        }
        else if ((instruction >> 12) == 0x9) {
            if (this->v[(instruction >> 8) & 0x000F] != this->v[(instruction >> 4) & 0x000F])
                this->pc += 2;
        }
        else if ((instruction >> 12) == 0xA)
            this->i = (instruction & 0x0FFF);
        else if ((instruction >> 12) == 0xB)
            this->pc = (instruction & 0x0FFF) + this->v[0x0] - 2;
        else if ((instruction >> 12) == 0xC)
            this->v[(instruction >> 8) & 0x000F] = ((rand() % 256) & (instruction & 0x00FF));
        else if ((instruction >> 12) == 0xD) {
            this->v[0xF] = this->draw_sprite(memory, this->v[(instruction >> 8) & 0x000F], this->v[(instruction >> 4) & 0x000F], (instruction & 0x000F));
            screen->update(memory);
        }
        else if ((instruction >> 12) == 0xE) {
            if ((instruction & 0x00FF) == 0x9E) {}
            else if ((instruction & 0x00FF) == 0xA1)
                this->pc += 2;
        }
        else if ((instruction >> 12) == 0xF) {
            if ((instruction & 0x00FF) == 0x07)
                this->v[(instruction >> 8) & 0x000F] = delay_timer->get_value();
            else if ((instruction & 0x00FF) == 0x0A) {
                this->pc -= 2;
            }
            else if ((instruction & 0x00FF) == 0x15)
                delay_timer->set(this->v[(instruction >> 8) & 0x000F]);
            else if ((instruction & 0x00FF) == 0x18)
                sound_timer->set(this->v[(instruction >> 8) & 0x000F]);
            else if ((instruction & 0x00FF) == 0x1E) {
                if (this->i + this->v[(instruction >> 8) & 0x000F] > 0xFFFF)
                    this->v[0xF] = 1;
                else
                    this->v[0xF] = 0;
                this->i += this->v[(instruction >> 8) & 0x000F];
            }
            else if ((instruction & 0x00FF) == 0x29)
                this->i = 5 * ((instruction >> 8) & 0x000F);
            else if ((instruction & 0x00FF) == 0x33) {
                unsigned char vv = this->v[(instruction >> 8) & 0x000F];

                memory[this->i] = 0;
                memory[this->i+1] = 0;
                memory[this->i+2] = 0;

                while (vv > 99) {
                    memory[this->i]++;
                    vv -= 100;
                }
                while (vv > 9) {
                    memory[this->i+1]++;
                    vv -= 10;
                }
                while (vv > 0) {
                    memory[this->i+2]++;
                    vv -= 1;
                }
            }
            else if ((instruction & 0x00FF) == 0x55) {
                for (size_t o = 0; o <= ((instruction >> 8) & 0x000F); o++)
                    memory[this->i + o] = this->v[o];
            }
            else if ((instruction & 0x00FF) == 0x65) {
                for (size_t o = 0; o <= ((instruction >> 8) & 0x000F); o++)
                    this->v[o] = memory[this->i + o];
            }
        }

        SDL_Delay(this->delay);
        // delay_timer->tick();
        // sound_timer->tick();

        this->pc += 2;
    }
};
