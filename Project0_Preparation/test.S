.global main
main:
    # load i=1, n=100
    li   t0, 1
    li   t1, 100
    # load res=0
    li   a0, 0

    # for (i=1; i<=n; i++) res += i
    bgt  t0, t1, end
loop:
    add  a0, a0, t0
    addi t0, t0, 1
    ble  t0, t1, loop

end:
	j end
