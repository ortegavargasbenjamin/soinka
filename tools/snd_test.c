// Prueba de la síntesis de sonido en el PC: renderiza cada efecto a un WAV en build/sfx/ e imprime duración,
// pico y RMS. Compilar: gcc -O2 -o build/snd_test tools/snd_test.c -lm  (incluye src/snd_synth.c directamente)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../src/snd_synth.c"

static void wav(const char *path, const short *s, int n)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    unsigned data = n * 2, riff = 36 + data, rate = SND_RATE, brate = SND_RATE * 2, fmt = 16;
    unsigned short pcm = 1, ch = 1, align = 2, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmt, 4, 1, f);
    fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&brate, 4, 1, f);
    fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f); fwrite(s, 2, n, f);
    fclose(f);
}

int main(void)
{
    int bad = 0;
    printf("%-20s %7s %7s %7s %7s  %s\n", "efecto", "dur(s)", "pico", "rms", "%>0.98", "estado");
    for (int id = 0; id < SFX_COUNT; id++) {
        short *s;
        int n = snd_synth(id, &s);
        if (n <= 0) { printf("%-20s FALLO\n", SFX_NAME[id]); bad++; continue; }
        double pk = 0, sum = 0;
        int hot = 0;
        for (int i = 0; i < n; i++) {
            double x = s[i] / 32768.0;
            if (fabs(x) > pk) pk = fabs(x);
            sum += x * x;
            if (fabs(x) > 0.98) hot++;
        }
        double rms = sqrt(sum / n), dur = (double)n / SND_RATE;
        double hotp = 100.0 * hot / n;
        int loop = snd_is_loop(id);
        const char *st = "ok";
        if (pk < 0.05 || rms < 0.002) st = "MUDO";
        else if (hotp > 5) st = "SATURADO";
        else if (loop ? (dur < 1 || dur > 3) : (id == SFX_SIREN ? dur > 3.0 : dur > 1.5)) st = "LARGO";
        double seam = 0;
        if (loop) {   // empalme: salto entre la última muestra y la primera frente al paso típico de la señal
            double step = 0;
            for (int i = 1; i < n; i++) step += fabs((double)s[i] - s[i - 1]);
            step /= (n - 1);
            seam = fabs((double)s[0] - s[n - 1]) / (step > 1 ? step : 1);
            if (seam > 12) st = "CLIC EN EMPALME";
        }
        printf("%-20s %7.2f %7.3f %7.3f %7.2f  %s%s", SFX_NAME[id], dur, pk, rms, hotp, st, loop ? " (bucle" : "");
        if (loop) printf(", salto/paso %.1f)", seam);
        printf("\n");
        if (strcmp(st, "ok")) bad++;
        char p[160];
        snprintf(p, sizeof(p), "build/sfx/%02d_%s.wav", id, SFX_NAME[id]);
        wav(p, s, n);
        free(s);
    }
    printf("%d efectos, %d con problema\n", SFX_COUNT, bad);
    return bad != 0;
}
