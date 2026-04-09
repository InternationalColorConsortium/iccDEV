iccDumpProfile -v calcOverMem_tget.icc > report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcOverMem_tput.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcOverMem_tsav.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_tput.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_tsav.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_tput.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_tsav.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_out.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect


iccDumpProfile -v calcUnderStack_curv.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_mtx.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_clut.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_tint.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_tJab.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_fJab.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_calc.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_elem.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect


iccDumpProfile -v calcUnderStack_copy.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_rotl.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_rotr.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_posd.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_flip.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_pop.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_sum.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_prod.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_min.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_max.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_and.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_or.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect


iccDumpProfile -v calcUnderStack_neg.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_add.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_sub.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_mul.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_div.icc >> report.txt"
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_mod.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_pow.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_gama.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_sadd.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_ssub.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_smul.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_sdiv.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_sq.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_sqrt.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_cb.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_cbrt.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_sign.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_abs.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_flor.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_ceil.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_trnc.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_rond.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_exp.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_log.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_ln.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_sin.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_cos.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_asin.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_acos.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_atan.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_atn2.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_ctop.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_ptoc.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_rnum.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_lt.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_le.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_eq.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_near.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_ge.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_gt.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_not.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_tLab.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_fLab.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_vmin.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_vmax.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_vand.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_vor.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect


iccDumpProfile -v calcUnderStack_solv.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

iccDumpProfile -v calcUnderStack_tran.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect


iccDumpProfile -v calcUnderStack_if.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect


iccDumpProfile -v calcUnderStack_sel.icc >> report.txt
@if ERRORLEVEL 0 goto incorrect

@echo .
@echo All profiles correctly tested as invalid
@goto end

:incorrect

@echo .
@echo ERROR! - Invalid Profile tested as valid

:end
@echo See results in report.txt
