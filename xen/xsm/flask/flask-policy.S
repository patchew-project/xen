        .section .init.rodata, "a", %progbits

/* const unsigned char xsm_flask_init_policy[] __initconst */
        .align 4
        .global xsm_flask_init_policy
xsm_flask_init_policy:
        .incbin "policy.bin"
.Lend:

        .type xsm_flask_init_policy, %object
        .size xsm_flask_init_policy, . - xsm_flask_init_policy

/* const unsigned int __initconst xsm_flask_init_policy_size */
        .align 4
        .global xsm_flask_init_policy_size
xsm_flask_init_policy_size:
        .long .Lend - xsm_flask_init_policy

        .type xsm_flask_init_policy_size, %object
        .size xsm_flask_init_policy_size, . - xsm_flask_init_policy_size
