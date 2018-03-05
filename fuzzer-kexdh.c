#include "fuzz.h"
#include "session.h"
#include "fuzz-wrapfd.h"
#include "debug.h"
#include "runopts.h"
#include "algo.h"
#include "bignum.h"

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
	static int once = 0;
	static struct key_context* keep_newkeys = NULL;
	#define NUM_PARAMS 800
	static struct kex_dh_param *dh_params[NUM_PARAMS];

	if (!once) {
		fuzz_common_setup();
		fuzz_svr_setup();

		keep_newkeys = (struct key_context*)m_malloc(sizeof(struct key_context));
		keep_newkeys->algo_kex = fuzz_get_algo(sshkex, "diffie-hellman-group14-sha256");
		keep_newkeys->algo_hostkey = DROPBEAR_SIGNKEY_ECDSA_NISTP256;
		ses.newkeys = keep_newkeys;

		/* Pre-generate parameters */
		int i;
		for (i = 0; i < NUM_PARAMS; i++) {
			dh_params[i] = gen_kexdh_param();
		}

		once = 1;
	}

	if (fuzz_set_input(Data, Size) == DROPBEAR_FAILURE) {
		return 0;
	}

	m_malloc_set_epoch(1);

	if (setjmp(fuzz.jmp) == 0) {
		/* Based on recv_msg_kexdh_init()/send_msg_kexdh_reply() 
		with DROPBEAR_KEX_NORMAL_DH */
		ses.newkeys = keep_newkeys;

		/* Choose from the collection of ecdh params */
		unsigned int e = buf_getint(fuzz.input);
		struct kex_dh_param * dh_param = dh_params[e % NUM_PARAMS];

		DEF_MP_INT(dh_e);
		m_mp_init(&dh_e);
		if (buf_getmpint(fuzz.input, &dh_e) != DROPBEAR_SUCCESS) {
			dropbear_exit("Bad kex value");
		}

		ses.kexhashbuf = buf_new(4);
		buf_putint(ses.kexhashbuf, 12345);
		kexdh_comb_key(dh_param, &dh_e, svr_opts.hostkey);

		/* kexhashbuf is freed in kexdh_comb_key */
		m_free(ses.dh_K);
		mp_clear(&dh_e);

		m_malloc_free_epoch(1, 0);
	} else {
		m_malloc_free_epoch(1, 1);
		TRACE(("dropbear_exit longjmped"))
		/* dropbear_exit jumped here */
	}

	return 0;
}
