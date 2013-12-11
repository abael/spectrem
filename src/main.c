/**
 * @file main.c Plot frequency spectrum from audio in WAV-files
 *
 * Copyright (C) 2010 Creytiv.com
 */

#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <math.h>
#include <re.h>
#include <rem.h>
#include "png_vf.h"
#include "kiss_fftr.h"


#define NUM_FFT 2048
#define NUM_FREQ (((NUM_FFT)/2)+1)


static unsigned long magv[NUM_FREQ];


static int read_wav(kiss_fftr_cfg fft, const char *infile)
{
	struct aufile *af_in = NULL;
	struct aufile_prm prm;
	size_t sampc_in_total = 0;
	size_t i;
	int err;

	err = aufile_open(&af_in, &prm, infile, AUFILE_READ);
	if (err) {
		re_fprintf(stderr, "%s: could not open input file (%m)\n",
			   infile, err);
		goto out;
	}

	if (prm.fmt != AUFMT_S16LE) {
		err = EINVAL;
		goto out;
	}

	re_printf("%s: %u Hz, %d channels\n", infile, prm.srate, prm.channels);

	for (;;) {
		int16_t sampv[NUM_FFT];
		size_t sz = sizeof(sampv);
		kiss_fft_cpx freqv[NUM_FREQ];

		err = aufile_read(af_in, (void *)sampv, &sz);
		if (err || !sz)
			break;

		if (sz != sizeof(sampv)) {
			re_printf("skipping last %zu samples\n", sz);
			break;
		}

		sampc_in_total += (sz/2);

		/* do FFT transform */
		kiss_fftr(fft, sampv, freqv);

		for (i=0; i<ARRAY_SIZE(freqv); i++) {

			kiss_fft_cpx cpx = freqv[i];
			magv[i] += sqrt(cpx.r * cpx.r + cpx.i * cpx.i);
		}
	}

	re_printf("read %u samples\n", sampc_in_total);

 out:
	if (err) {
		re_fprintf(stderr, "file read error: %m\n", err);
	}

	mem_deref(af_in);

	return err;
}


static int plot_spectrum(const char *filename_png)
{
	struct vidframe *vf = NULL;
	struct vidsz sz = {NUM_FREQ+1, NUM_FREQ/2};
	unsigned long peak_mag = 0;
	size_t peak_bin = 0;
	size_t i;
	unsigned x;
	int err;

	err = vidframe_alloc(&vf, VID_FMT_RGB32, &sz);
	if (err)
		goto out;

	/* find the peak amplitude and its bin */
	for (i=0; i<NUM_FREQ; i++) {

		if (magv[i] > peak_mag) {
			peak_mag = magv[i];
			peak_bin = i;
		}
	}
	re_printf("peak magnitude is %u in bin %u\n", peak_mag, peak_bin);

	vidframe_fill(vf, 255, 255, 255);

	for (x=0; x<NUM_FREQ; x++) {

		unsigned h;

		h = (unsigned)((sz.h-1) * 1.0 * magv[x] / peak_mag);

		vidframe_draw_vline(vf, x, sz.h-1-h, h, 255, 0, 0);
	}

	err = png_save_vidframe(vf, filename_png);
	if (err)
		goto out;

 out:
	mem_deref(vf);

	return err;
}


static void usage(void)
{
	(void)re_fprintf(stderr,
			 "spectrem -h  input.wav output.png\n");
	(void)re_fprintf(stderr, "\t-h            Show summary of options\n");
}


int main(int argc, char *argv[])
{
	const char *filename_wav, *filename_png;
	kiss_fftr_cfg fft;
	int err = 0;

	for (;;) {

		const int c = getopt(argc, argv, "h");
		if (0 > c)
			break;

		switch (c) {

		case '?':
			err = EINVAL;
			/*@fallthrough@*/
		case 'h':
			usage();
			return err;
		}
	}

	if (argc < 3 || argc != (optind + 2)) {
		usage();
		return -EINVAL;
	}

	filename_wav = argv[optind++];
	filename_png = argv[optind++];

	fft = kiss_fftr_alloc(NUM_FFT, 0, 0, 0);
	if (!fft) {
		err = ENOMEM;
		goto out;
	}

	err = read_wav(fft, filename_wav);
	if (err)
		goto out;
 
	err = plot_spectrum(filename_png);
	if (err)
		goto out;

 out:
	if (fft)
		kiss_fftr_free(fft);

	tmr_debug();
	mem_debug();

	return err;
}
