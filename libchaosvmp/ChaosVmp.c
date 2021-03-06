#include "udis86.h"
#include "Support.h"
#include "ChaosVmp.h"

/*
 * 参数:
 *	pOpcode:要产生的OP表
 *	pOpcodeExchange:转换OP的函数指针
 *
 * 介绍:
 *	产生一张随机的OP编码表,如果pOpcodeExchange不为空则使用pOpcodeExchange提供的函数指针,如果为空则是一个随机字节对256个表进行异或操作
 */
__void __API__ GenerateRandOpcodeTable(__byte *pOpcode, FPOpcodeExchange pOpcodeExchange) {
	__integer i = 0;
	__byte bKey = (__byte)GetRandDword();
	for (i = 0; i < 256; i++) {
		if (pOpcodeExchange)
			*(pOpcode + i) = pOpcodeExchange(*(pOpcode + i));
		else
			*(pOpcode + i) = i ^ bKey;
	}
}

/*
 * 参数:
 *	pModRm:要输出的MODRM指针
 *	pModRmExchage:MODRM转换函数指针
 *
 * 介绍:
 *	产生重新编码的MODRM字节,如果pModRmExchage不为空则使用它,如果为空则,则随机抽取g_OpcodeSwitchTable中的一组顺序进行编码
 */

// 使用此表进行随机编码
__byte g_OpcodeSwitchTable[6][3] = {
	"\x00\x01\x02",//000102
	"\x00\x02\x01",//000201
	"\x01\x00\x02",//010002
	"\x01\x02\x00",//010200
	"\x02\x00\x01",//020001
	"\x02\x01\x00"//020100
};

__byte * __API__ GenerateRandModRmTable(__byte *pModRm, FPModRmSibExchage pModRmExchage) {
	if (pModRmExchage)
		return pModRmExchage(pModRm);
	else {
		__byte i = (__byte)(GetRandDword() % 6);//0 - 5
		__logic_memcpy__(pModRm, g_OpcodeSwitchTable[i], sizeof(__byte) * 3);
	}
	return pModRm;
}

__byte * __API__ GenerateRandSibTable(__byte *pSib, FPModRmSibExchage pModRmSibExchage) {
	return GenerateRandModRmTable(pSib, pModRmSibExchage);
}

/*
 * 参数:
 *  bByte:当前OPCODE
 * 
 * 介绍:
 *	判断此字节是否是前缀指令,如果是则返回TRUE
 */
__INLINE__ __bool __INTERNAL_FUNC__ ThisByteIsPrefix(__byte bByte) {
	if ((bByte == 0xF0) || (bByte == 0xF2) || (bByte == 0xF3) ||
		(bByte == 0x2E) || (bByte == 0x36) || (bByte == 0x3E) ||
		(bByte == 0x26) || (bByte == 0x64) || (bByte == 0x65) ||
		(bByte == 0x2E) || (bByte == 0x3E) || (bByte == 0x66) ||
		(bByte == 0x67)) return TRUE;
	return FALSE;
}

/*
 * 参数:
 *  pud_obj:当前指令的反汇编结构
 *	iError:错误的类型
 *	ofOffset:当前指令相对于函数头的偏移
 * 
 * 介绍:
 *	在保护指令时出错后的处理,如果返回TRUE则表示此指令为严重错误,可以直接退出,FALSE表示警告
 */
#define __VMP_INST_ERROR__(x)					(x & 0x80)
__INLINE__ __bool __INTERNAL_FUNC__ OnVmpThisInstructionError(ud_t *pud_obj, __integer iError, __offset ofOffset) {
	// 退出判断
	if (iError == __VMP_INST_NOT_SUPPORT__) {
		__integer iInstLen  = 0;
		__char *pszInstHex = NULL;
		__char *pszInst = NULL;

		iInstLen = (__integer)ud_insn_len(pud_obj);
		pszInstHex = (__char *)ud_insn_hex(pud_obj);
		pszInst = (__char *)ud_insn_asm(pud_obj);
		VmpThisInstructionErrorNotification(iError, pszInstHex, pszInst, iInstLen, ofOffset);
		return TRUE;
	}
}

/*
 * 参数:
 *  pud_obj:当前指令的反汇编结构
 *  pInst:当前指令的字节码
 *  pOpcode1Tbl:转换后的第一张OPCODE表
 *  pOpcode2Tbl:转换后的第二张OPCODE表
 *	pModRmTbl:转换后的MODRM表
 *	pSibTbl:转换后的SIB表
 * 
 * 介绍:
 *	虚拟保护这条指令,并返回"偏移+立即数"部分的长度,如果不支持此条指令则返回FALSE(0)
 */
#include "VmpThisInstruction.c"
__INLINE__ __integer __INTERNAL_FUNC__ VmpThisInstruction(ud_t *pud_obj, __memory pInst, __byte *pOpcode1Tbl, \
														  __byte *pOpcode2Tbl, __byte *pModRmTbl, __byte *pSibTbl) {
	__integer iRet = 0;
	__memory pCurrInst = pInst;//指向当前指令的字节码缓存
	__integer iInstLen = pud_obj->inp_ctr;//当前指令长度
	__integer iRemainLen = iInstLen;//当前指令剩余长度
	__integer iCount = 0;
	__byte bOpcode = *(pInst + iCount);//获取当前的字节码

	// 遍历这条指令的字节码缓存并处理前缀,如果发现是前缀字节码,则使用变形后的第一张OP表进行重新编码,直到处理完前缀为止
	while (ThisByteIsPrefix(bOpcode)) {
		*(pCurrInst + iCount) = *(pOpcode1Tbl + bOpcode);
		iCount++;
		bOpcode = *(pCurrInst + iCount);//读取下一个字节
	}
	
	// 按照指令进行处理
	iRemainLen -= iCount;//除去前缀的纯指令长度
	pCurrInst += iCount;//指向纯指令部分
	switch (pud_obj->mnemonic) {
	case UD_I3dnow:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iaaa:{
		iRet = VmpThisInstruction_Iaaa(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iaad:{
		iRet = VmpThisInstruction_Iaad(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iaam:{
		iRet = VmpThisInstruction_Iaam(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iaas:{
		iRet = VmpThisInstruction_Iaas(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iadc:{
		iRet = VmpThisInstruction_Iadc(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iadd:{
		iRet = VmpThisInstruction_Iadd(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iaddpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iaddps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iaddsd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iaddss:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iaddsubpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iaddsubps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iand:{
		iRet = VmpThisInstruction_Iand(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iandpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iandps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iandnpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iandnps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iarpl:{
		iRet = VmpThisInstruction_Iarpl(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Imovsxd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ibound:{
		iRet = VmpThisInstruction_Ibound(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ibsf:{
		iRet = VmpThisInstruction_Ibsf(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ibsr:{
		iRet = VmpThisInstruction_Ibsr(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ibswap:{
		iRet = VmpThisInstruction_Ibswap(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ibt:{
		iRet = VmpThisInstruction_Ibt(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ibtc:{
		iRet = VmpThisInstruction_Ibtc(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ibtr:{
		iRet = VmpThisInstruction_Ibtr(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ibts:{
		iRet = VmpThisInstruction_Ibts(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icall:{
		iRet = VmpThisInstruction_Icall(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icbw:{
		iRet = VmpThisInstruction_Icbw(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icwde:{
		iRet = VmpThisInstruction_Icwde(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icdqe:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iclc:{
		iRet = VmpThisInstruction_Iclc(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icld:{
		iRet = VmpThisInstruction_Icld(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iclflush:{
		iRet = VmpThisInstruction_Iclflush(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iclgi:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icli:{
		iRet = VmpThisInstruction_Icli(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iclts:{
		iRet = VmpThisInstruction_Iclts(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmc:{
		iRet = VmpThisInstruction_Icmc(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmovo:{
		iRet = VmpThisInstruction_Icmovo(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmovno:{
		iRet = VmpThisInstruction_Icmovno(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmovb:{
		iRet = VmpThisInstruction_Icmovb(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmovae:{
		iRet = VmpThisInstruction_Icmovae(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmovz:{
		iRet = VmpThisInstruction_Icmovz(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmovnz:{
		iRet = VmpThisInstruction_Icmovnz(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmovbe:{
		iRet = VmpThisInstruction_Icmovbe(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmova:{
		iRet = VmpThisInstruction_Icmova(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmovs:{
		iRet = VmpThisInstruction_Icmovs(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmovns:{
		iRet = VmpThisInstruction_Icmovns(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmovp:{
		iRet = VmpThisInstruction_Icmovp(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmovnp:{
		iRet = VmpThisInstruction_Icmovnp(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmovl:{
		iRet = VmpThisInstruction_Icmovl(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmovge:{
		iRet = VmpThisInstruction_Icmovge(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmovle:{
		iRet = VmpThisInstruction_Icmovle(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmovg:{
		iRet = VmpThisInstruction_Icmovg(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmp:{
		iRet = VmpThisInstruction_Icmp(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmppd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icmpps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icmpsb:{
		iRet = VmpThisInstruction_Icmpsb(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmpsw:{
		iRet = VmpThisInstruction_Icmpsw(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmpsd:{
		iRet = VmpThisInstruction_Icmpsd(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmpsq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icmpss:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icmpxchg:{
		iRet = VmpThisInstruction_Icmpxchg(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icmpxchg8b:{
		iRet = VmpThisInstruction_Icmpxchg8b(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icomisd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icomiss:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icpuid:{
		iRet = VmpThisInstruction_Icpuid(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icvtdq2pd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvtdq2ps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvtpd2dq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvtpd2pi:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvtpd2ps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvtpi2ps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvtpi2pd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvtps2dq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvtps2pi:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvtps2pd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvtsd2si:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvtsd2ss:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvtsi2ss:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvtss2si:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvtss2sd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvttpd2pi:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvttpd2dq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvttps2dq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvttps2pi:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvttsd2si:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvtsi2sd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icvttss2si:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Icwd:{
		iRet = VmpThisInstruction_Icwd(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icdq:{
		iRet = VmpThisInstruction_Icdq(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Icqo:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Idaa:{
		iRet = VmpThisInstruction_Idaa(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Idas:{
		iRet = VmpThisInstruction_Idas(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Idec:{
		iRet = VmpThisInstruction_Idec(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Idiv:{
		iRet = VmpThisInstruction_Idiv(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Idivpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Idivps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Idivsd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Idivss:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iemms:{
		iRet = VmpThisInstruction_Iemms(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ienter:{
		iRet = VmpThisInstruction_Ienter(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_If2xm1:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifabs:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifadd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifaddp:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifbld:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifbstp:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifchs:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifclex:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifcmovb:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifcmove:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifcmovbe:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifcmovu:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifcmovnb:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifcmovne:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifcmovnbe:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifcmovnu:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifucomi:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifcom:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifcom2:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifcomp3:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifcomi:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifucomip:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifcomip:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifcomp:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifcomp5:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifcompp:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifcos:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifdecstp:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifdiv:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifdivp:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifdivr:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifdivrp:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifemms:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iffree:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iffreep:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ificom:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ificomp:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifild:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifncstp:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifninit:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifiadd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifidivr:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifidiv:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifisub:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifisubr:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifist:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifistp:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifisttp:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifld:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifld1:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifldl2t:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifldl2e:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifldlpi:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifldlg2:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifldln2:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifldz:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifldcw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifldenv:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifmul:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifmulp:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifimul:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifnop:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifpatan:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifprem:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifprem1:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifptan:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifrndint:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifrstor:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifnsave:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifscale:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifsin:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifsincos:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifsqrt:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifstp:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifstp1:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifstp8:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifstp9:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifst:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifnstcw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifnstenv:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifnstsw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifsub:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifsubp:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifsubr:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifsubrp:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iftst:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifucom:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifucomp:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifucompp:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifxam:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifxch:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifxch4:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifxch7:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifxrstor:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifxsave:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifpxtract:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifyl2x:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ifyl2xp1:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ihaddpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ihaddps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ihlt:{
		iRet = VmpThisInstruction_Ihlt(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ihsubpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ihsubps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iidiv:{
		iRet = VmpThisInstruction_Iidiv(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iin:{
		iRet = VmpThisInstruction_Iin(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iimul:{
		iRet = VmpThisInstruction_Iimul(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iinc:{
		iRet = VmpThisInstruction_Iinc(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iinsb:{
		iRet = VmpThisInstruction_Iinsb(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iinsw:{
		iRet = VmpThisInstruction_Iinsw(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iinsd:{
		iRet = VmpThisInstruction_Iinsd(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iint1:{
		iRet = VmpThisInstruction_Iint1(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iint3:{
		iRet = VmpThisInstruction_Iint3(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iint:{
		iRet = VmpThisInstruction_Iint(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iinto:{
		iRet = VmpThisInstruction_Iinto(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iinvd:{
		iRet = VmpThisInstruction_Iinvd(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iinvlpg:{
		iRet = VmpThisInstruction_Iinvlpg(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iinvlpga:{
		iRet = VmpThisInstruction_Iinvlpg(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iiretw:{
		iRet = VmpThisInstruction_Iiretw(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iiretd:{
		iRet = VmpThisInstruction_Iiretd(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iiretq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ijo:{
		iRet = VmpThisInstruction_Ijo(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ijno:{
		iRet = VmpThisInstruction_Ijno(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ijb:{
		iRet = VmpThisInstruction_Ijb(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ijae:{
		iRet = VmpThisInstruction_Ijae(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ijz:{
		iRet = VmpThisInstruction_Ijz(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ijnz:{
		iRet = VmpThisInstruction_Ijnz(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ijbe:{
		iRet = VmpThisInstruction_Ijbe(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ija:{
		iRet = VmpThisInstruction_Ija(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ijs:{
		iRet = VmpThisInstruction_Ijs(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ijns:{
		iRet = VmpThisInstruction_Ijns(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ijp:{
		iRet = VmpThisInstruction_Ijp(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ijnp:{
		iRet = VmpThisInstruction_Ijnp(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ijl:{
		iRet = VmpThisInstruction_Ijl(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ijge:{
		iRet = VmpThisInstruction_Ijge(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ijle:{
		iRet = VmpThisInstruction_Ijle(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ijg:{
		iRet = VmpThisInstruction_Ijg(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ijcxz:{
		iRet = VmpThisInstruction_Ijcxz(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ijecxz:{
		iRet = VmpThisInstruction_Ijecxz(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ijrcxz:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ijmp:{
		iRet = VmpThisInstruction_Ijmp(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ilahf:{
		iRet = VmpThisInstruction_Ilahf(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ilar:{
		iRet = VmpThisInstruction_Ilar(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ilddqu:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ildmxcsr:{
		iRet = VmpThisInstruction_Ildmxcsr(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ilds:{
		iRet = VmpThisInstruction_Ilds(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ilea:{
		iRet = VmpThisInstruction_Ilea(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iles:{
		iRet = VmpThisInstruction_Iles(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ilfs:{
		iRet = VmpThisInstruction_Ilfs(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ilgs:{
		iRet = VmpThisInstruction_Ilgs(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ilidt:{
		iRet = VmpThisInstruction_Ilidt(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ilss:{
		iRet = VmpThisInstruction_Ilss(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ileave:{
		iRet = VmpThisInstruction_Ileave(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ilfence:{
		iRet = VmpThisInstruction_Ilfence(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ilgdt:{
		iRet = VmpThisInstruction_Ilgdt(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Illdt:{
		iRet = VmpThisInstruction_Illdt(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ilmsw:{
		iRet = VmpThisInstruction_Ilmsw(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ilock:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ilodsb:{
		iRet = VmpThisInstruction_Ilodsb(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ilodsw:{
		iRet = VmpThisInstruction_Ilodsw(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ilodsd:{
		iRet = VmpThisInstruction_Ilodsd(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ilodsq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iloopnz:{
		iRet = VmpThisInstruction_Iloopnz(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iloope:{
		iRet = VmpThisInstruction_Iloope(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iloop:{
		iRet = VmpThisInstruction_Iloop(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ilsl:{
		iRet = VmpThisInstruction_Ilsl(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iltr:{
		iRet = VmpThisInstruction_Iltr(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Imaskmovq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imaxpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imaxps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imaxsd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imaxss:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imfence:{
		iRet = VmpThisInstruction_Imfence(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iminpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iminps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iminsd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iminss:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imonitor:{
		iRet = VmpThisInstruction_Imonitor(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Imov:{
		iRet = VmpThisInstruction_Imov(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Imovapd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovaps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovddup:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovdqa:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovdqu:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovdq2q:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovhpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovhps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovlhps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovlpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovlps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovhlps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovmskpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovmskps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovntdq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovnti:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovntpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovntps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovntq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovqa:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovq2dq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovsb:{
		iRet = VmpThisInstruction_Imovsb(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Imovsw:{
		iRet = VmpThisInstruction_Imovsw(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Imovsd:{
		iRet = VmpThisInstruction_Imovsd(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Imovsq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovsldup:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovshdup:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovss:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovsx:{
		iRet = VmpThisInstruction_Imovsx(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Imovupd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovups:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imovzx:{
		iRet = VmpThisInstruction_Imovzx(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Imul:{
		iRet = VmpThisInstruction_Imul(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Imulpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imulps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imulsd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imulss:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Imwait:{
		iRet = VmpThisInstruction_Imwait(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ineg:{
		iRet = VmpThisInstruction_Ineg(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Inop:{
		iRet = VmpThisInstruction_Inop(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Inot:{
		iRet = VmpThisInstruction_Inot(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ior:{
		iRet = VmpThisInstruction_Ior(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iorpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iorps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iout:{
		iRet = VmpThisInstruction_Iout(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ioutsb:{
		iRet = VmpThisInstruction_Ioutsb(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ioutsw:{
		iRet = VmpThisInstruction_Ioutsw(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ioutsd:{
		iRet = VmpThisInstruction_Ioutsd(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ioutsq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipacksswb:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipackssdw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipackuswb:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipaddb:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipaddw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipaddq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipaddsb:{
		return __VMP_INST_NOT_SUPPORT__;	
	}break;
	case UD_Ipaddsw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipaddusb:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipaddusw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipand:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipandn:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipause:{
		iRet = VmpThisInstruction_Ipause(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ipavgb:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipavgw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipcmpeqb:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipcmpeqw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipcmpeqd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipcmpgtb:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipcmpgtw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipcmpgtd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipextrw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipinsrw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipmaddwd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipmaxsw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipmaxub:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipminsw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipminub:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipmovmskb:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipmulhuw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipmulhw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipmullw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipmuludq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipop:{
		iRet = VmpThisInstruction_Ipop(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ipopa:{
		iRet = VmpThisInstruction_Ipopa(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ipopad:{
		iRet = VmpThisInstruction_Ipopad(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ipopfw:{
		iRet = VmpThisInstruction_Ipopfw(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ipopfd:{
		iRet = VmpThisInstruction_Ipopfd(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ipopfq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipor:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iprefetch:{
		iRet = VmpThisInstruction_Iprefetch(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iprefetchnta:{
		iRet = VmpThisInstruction_Iprefetchnta(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iprefetcht0:{
		iRet = VmpThisInstruction_Iprefetcht0(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iprefetcht1:{
		iRet = VmpThisInstruction_Iprefetcht1(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iprefetcht2:{
		iRet = VmpThisInstruction_Iprefetcht2(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ipsadbw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipshufd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipshufhw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipshuflw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipshufw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipslldq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipsllw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipslld:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipsllq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipsraw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipsrad:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipsrlw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipsrld:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipsrlq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipsrldq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipsubb:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipsubw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipsubd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipsubq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipsubsb:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipsubsw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipsubusb:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipsubusw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipunpckhbw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipunpckhwd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipunpckhdq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipunpckhqdq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipunpcklbw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipunpcklwd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipunpckldq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipunpcklqdq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipi2fw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipi2fd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipf2iw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipf2id:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipfnacc:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipfpnacc:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipfcmpge:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipfmin:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipfrcp:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipfrsqrt:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipfsub:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipfadd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipfcmpgt:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipfmax:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipfrcpit1:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipfrspit1:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipfsubr:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipfacc:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipfcmpeq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipfmul:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipfrcpit2:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipmulhrw:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipswapd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipavgusb:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipush:{
		iRet = VmpThisInstruction_Ipush(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ipusha:{
		iRet = VmpThisInstruction_Ipusha(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ipushad:{
		iRet = VmpThisInstruction_Ipushad(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ipushfw:{
		iRet = VmpThisInstruction_Ipushfw(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ipushfd:{
		iRet = VmpThisInstruction_Ipushfd(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ipushfq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ipxor:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ircl:{
		iRet = VmpThisInstruction_Ircl(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ircr:{
		iRet = VmpThisInstruction_Ircr(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Irol:{
		iRet = VmpThisInstruction_Irol(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iror:{
		iRet = VmpThisInstruction_Iror(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ircpps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ircpss:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Irdmsr:{
		iRet = VmpThisInstruction_Irdmsr(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Irdpmc:{
		iRet = VmpThisInstruction_Irdpmc(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Irdtsc:{
		iRet = VmpThisInstruction_Irdtsc(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Irdtscp:{
		iRet = VmpThisInstruction_Irdtscp(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Irepne:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Irep:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iret:{
		iRet = VmpThisInstruction_Iret(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iretf:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Irsm:{
		iRet = VmpThisInstruction_Irsm(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Irsqrtps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Irsqrtss:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Isahf:{
		iRet = VmpThisInstruction_Isahf(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isal:{
		iRet = VmpThisInstruction_Isal(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isalc:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Isar:{
		iRet = VmpThisInstruction_Isar(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ishl:{
		iRet = VmpThisInstruction_Ishl(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ishr:{
		iRet = VmpThisInstruction_Ishr(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isbb:{
		iRet = VmpThisInstruction_Isbb(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iscasb:{
		iRet = VmpThisInstruction_Iscasb(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iscasw:{
		iRet = VmpThisInstruction_Iscasw(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iscasd:{
		iRet = VmpThisInstruction_Iscasd(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iscasq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iseto:{
		iRet = VmpThisInstruction_Iseto(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isetno:{
		iRet = VmpThisInstruction_Isetno(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isetb:{
		iRet = VmpThisInstruction_Isetb(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isetnb:{
		iRet = VmpThisInstruction_Isetnb(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isetz:{
		iRet = VmpThisInstruction_Isetz(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isetnz:{
		iRet = VmpThisInstruction_Isetnz(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isetbe:{
		iRet = VmpThisInstruction_Isetbe(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iseta:{
		iRet = VmpThisInstruction_Iseta(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isets:{
		iRet = VmpThisInstruction_Isets(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isetns:{
		iRet = VmpThisInstruction_Isetns(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isetp:{
		iRet = VmpThisInstruction_Isetp(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isetnp:{
		iRet = VmpThisInstruction_Isetnp(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isetl:{
		iRet = VmpThisInstruction_Isetl(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isetge:{
		iRet = VmpThisInstruction_Isetge(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isetle:{
		iRet = VmpThisInstruction_Isetle(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isetg:{
		iRet = VmpThisInstruction_Isetg(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isfence:{
		iRet = VmpThisInstruction_Isfence(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isgdt:{
		iRet = VmpThisInstruction_Isgdt(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ishld:{
		iRet = VmpThisInstruction_Ishld(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ishrd:{
		iRet = VmpThisInstruction_Ishrd(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ishufpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ishufps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Isidt:{
		iRet = VmpThisInstruction_Isidt(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isldt:{
		iRet = VmpThisInstruction_Isldt(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ismsw:{
		iRet = VmpThisInstruction_Ismsw(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isqrtps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Isqrtpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Isqrtsd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Isqrtss:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Istc:{
		iRet = VmpThisInstruction_Istc(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Istd:{
		iRet = VmpThisInstruction_Istd(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Istgi:{
		iRet = __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Isti:{
		iRet = VmpThisInstruction_Isti(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iskinit:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Istmxcsr:{
		iRet = VmpThisInstruction_Istmxcsr(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Istosb:{
		iRet = VmpThisInstruction_Istosb(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Istosw:{
		iRet = VmpThisInstruction_Istosw(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Istosd:{
		iRet = VmpThisInstruction_Istosd(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Istosq:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Istr:{
		iRet = VmpThisInstruction_Istr(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isub:{
		iRet = VmpThisInstruction_Isub(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isubpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Isubps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Isubsd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Isubss:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iswapgs:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Isyscall:{
		iRet = VmpThisInstruction_Isyscall(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isysenter:{
		iRet = VmpThisInstruction_Isysenter(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isysexit:{
		iRet = VmpThisInstruction_Isysexit(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Isysret:{
		iRet = VmpThisInstruction_Isysret(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Itest:{
		iRet = VmpThisInstruction_Itest(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iucomisd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iucomiss:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iud2:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iunpckhpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iunpckhps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iunpcklps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iunpcklpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iverr:{
		iRet = VmpThisInstruction_Iverr(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iverw:{
		iRet = VmpThisInstruction_Iverw(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ivmcall:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ivmclear:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ivmxon:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ivmptrld:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ivmptrst:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ivmresume:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ivmxoff:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ivmrun:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ivmmcall:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ivmload:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ivmsave:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iwait:{
		iRet = VmpThisInstruction_Iwait(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Iwbinvd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iwrmsr:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ixadd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ixchg:{
		iRet = VmpThisInstruction_Ixchg(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ixlatb:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ixor:{
		iRet = VmpThisInstruction_Ixor(pud_obj, pCurrInst, iRemainLen, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);
	}break;
	case UD_Ixorpd:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ixorps:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Idb:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Iinvalid:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Id3vil:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Ina:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Igrp_reg:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Igrp_rm:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Igrp_vendor:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Igrp_x87:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Igrp_mode:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Igrp_osize:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Igrp_asize:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Igrp_mod:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	case UD_Inone:{
		return __VMP_INST_NOT_SUPPORT__;
	}break;
	}
	
	return iRet;
}

/*
 * 参数:
 *	pud_obj:udis86的反汇编结构,记录了反汇编后的指令信息
 *
 * 介绍:
 *	判断当前反汇编后的指令是否是跳转指令,如果是则返回TRUE
 */
__INLINE__ __bool __INTERNAL_FUNC__ VmpThisInstructionIsFlowInstruction(ud_t *pud_obj) {
	switch (pud_obj->mnemonic) {
	case UD_Ijo:
	case UD_Ijno:
	case UD_Ijb:
	case UD_Ijae:
	case UD_Ijz:
	case UD_Ijnz:
	case UD_Ijbe:
	case UD_Ija:
	case UD_Ijs:
	case UD_Ijns:;
	case UD_Ijp:
	case UD_Ijnp:
	case UD_Ijl:
	case UD_Ijge:
	case UD_Ijle:
	case UD_Ijg:
	case UD_Ijcxz:
	case UD_Ijecxz:
	case UD_Iloopnz:
	case UD_Iloope:
	case UD_Iloop:
	case UD_Ijmp:
	case UD_Icall:{
		return TRUE;
	}break;
	}
	return FALSE;
}

__INLINE__ PCHAOSVMP_JMPTARGET __INTERNAL_FUNC__ CreateChaosVmpInstRecord() {
	PCHAOSVMP_JMPTARGET pJmpTarget = __logic_new__(CHAOSVMP_JMPTARGET, 1);
	__logic_memset__(pJmpTarget, 0, sizeof(CHAOSVMP_JMPTARGET));
	return pJmpTarget;
}

/*
 * 参数:
 *  pJmpTargetInst:跳转目标记录结构
 *  ofRVA:跳转目标的内存地址RVA
 *	pAddress:跳转目标的文件地址
 *	iPrevInstRandLen:跳转目标的上一条指令长度范围的随机长度,如果当前跳转目标地址是函数头地址则设置为0
 * 
 * 介绍:
 *	记录跳转目标结构的信息,如果跳转目标地址的内存地址,文件地址,以及跳转目标地址的上一条指令长度范围的随机长度
 */
__INLINE__ __void __INTERNAL_FUNC__ SetChaosVmpInstRecord(PCHAOSVMP_JMPTARGET_INST pJmpTargetInst, __offset ofRVA, \
														  __offset ofProcRVA, __memory pAddress, __integer iPrevInstRandLen) {
	__offset ofOffsetByProcedure = ofRVA - ofProcRVA;//求得目标地址离函数头的偏移

	pJmpTargetInst->ofRVA = ofRVA;
	pJmpTargetInst->pAddress = pAddress;
	pJmpTargetInst->ofOffsetByProcedure = ofOffsetByProcedure;
	pJmpTargetInst->iPrevInstRandLen = iPrevInstRandLen;
}

/*
 * 参数:
 *  pJmpTarget:跳转目标记录结构
 *  ofRVA:跳转目标的内存地址RVA
 * 
 * 介绍:
 *	判断ofRVA所指的地址是否已经存在于pJmpTarget,也就是表示此目标地址已经有其他跳转指令跳入,如果已经进行过分析返回处理过的跳转目标指令结构
 *	如果无则直接返回NULL
 */
__INLINE__ PCHAOSVMP_JMPTARGET_INST __INTERNAL_FUNC__ ThisInstructionIsInDataBase(PCHAOSVMP_JMPTARGET pJmpTarget, __offset ofRVA) {
	__integer i = 0;
	for (i = 0; i < pJmpTarget->iAddressCount; i++) {
		if (ofRVA == pJmpTarget->pInstList[i].ofRVA)
			return &(pJmpTarget->pInstList[i]);
	}
	return NULL;
}

/*
 * 参数:
 *  pChaosVmpJmpTargetPoint:指向跳转目标记录结构的指针
 * 
 * 介绍:
 *	释放跳转目标结构的内存
 */
__INLINE__ __void __INTERNAL_FUNC__ ReleaseChaosVmpJmpTarget(PCHAOSVMP_JMPTARGET *pChaosVmpJmpTargetPoint) {
	__logic_delete__(*pChaosVmpJmpTargetPoint);
	*pChaosVmpJmpTargetPoint = NULL;
}

/*
 * 参数:
 *  pud_obj:跳转指令记录结构
 *  pProcedure:函数的文件地址
 *  iSize:函数的大小
 *  ofProcedureRVA:函数的内存地址RVA
 *	ofCurrOffset:当前指令离函数头的偏移
 *	pJmpTarget:跳转记录结构
 *	iInstIndex:指令在当前函数中的索引
 * 
 * 介绍:
 *	计算当前跳转指令的基本信息,跳转目标内存地址,文件地址以及指令长度等,并且处理跳转指令的目标地址,因为跳转目标地址的跳转指令,可能并非只有当前指令一个
 *	跳转目标地址所在指令,只能使用它的上一条指令的密文作为密钥对它自己进行加密,如果在当前跳转指令之前已经有其他跳转指令的目标地址同它一样,则跳过并直接
 *	返回0,如果不是则重新分析这个函数,当重新找到跳转目标地址后取一段它上一条指令长度范围内的随机长度,如果跳转目标是函数头(应该不会出现)则将字段设置为0,
 *	然后将这些数据存入跳转目标记录结构,并返回这个随机的数值
 */
#define __ChaosVmpJmpUpOffset__(Offset, InstLen) ((~(Offset) + 1) - (InstLen))
#define __ChaosVmpJmpDownOffset__(Offset, InstLen) ((Offset) + (InstLen))
__INLINE__ __integer __INTERNAL_FUNC__ VmpRecordJmpTargetInfo(ud_t *pud_obj, __memory pProcedure, __integer iSize, __offset ofProcedureRVA, \
															__offset ofCurrOffset, PCHAOSVMP_JMPTARGET pJmpTarget, __integer iInstIndex){
	__offset ofTargetRVA = 0;//跳转目标的内存地址RVA
	__offset ofCurrRVA = ofProcedureRVA + ofCurrOffset;//当前指令的内存地址RVA
	__integer iPrevInstRandLen = 0, iCurrInstLen = 0;//上一条指令,以及当前指令的长度
	__offset ofNowRVA = 0;//在分析跳转目标地址时使用,保存当前指令的内存地址RVA
	__memory pTargetAddress = NULL;//跳转目标的文件地址
	__memory pCurrAddress = pProcedure + ofCurrOffset;//当前指令的文件地址
	PCHAOSVMP_JMPTARGET_INST pJmpTargetInst = NULL;//当前跳转记录指令结构
	ud_t udObjNow;

	if (pud_obj->operand[0].type == UD_OP_JIMM) {
		if (pud_obj->operand[0].size == 8) {
			// 8位
			__byte bOffset = pud_obj->operand[0].lval.ubyte;
			if (__IsNegative8__(bOffset)) {
				bOffset = __ChaosVmpJmpUpOffset__(bOffset, pud_obj->inp_ctr);
				ofTargetRVA = ofCurrRVA - (__address)bOffset;
				pTargetAddress = pCurrAddress - bOffset;
			} else {
				bOffset = __ChaosVmpJmpDownOffset__(bOffset, pud_obj->inp_ctr);
				ofTargetRVA = ofCurrRVA + (__address)bOffset;
				pTargetAddress = pCurrAddress + bOffset;
			}
		} else if (pud_obj->operand[0].size == 16) {
			// 16位
			__word wOffset = pud_obj->operand[0].lval.uword;
			if (__IsNegative16__(wOffset)) {
				wOffset = __ChaosVmpJmpUpOffset__(wOffset, pud_obj->inp_ctr);
				ofTargetRVA = ofCurrRVA - (__address)wOffset;
				pTargetAddress = pCurrAddress - wOffset;
			} else {
				wOffset = __ChaosVmpJmpDownOffset__(wOffset, pud_obj->inp_ctr);
				ofTargetRVA = ofCurrRVA + (__address)wOffset;
				pTargetAddress = pCurrAddress + wOffset;
			}/* end else */
		} else if (pud_obj->operand[0].size == 32) {
			// 32位
			__dword dwOffset = pud_obj->operand[0].lval.udword;
			if (__IsNegative32__(dwOffset)) {
				dwOffset = __ChaosVmpJmpUpOffset__(dwOffset, pud_obj->inp_ctr);
				ofTargetRVA = ofCurrRVA - (__address)dwOffset;
				pTargetAddress = pCurrAddress - dwOffset;
			} else {
				dwOffset = __ChaosVmpJmpDownOffset__(dwOffset, pud_obj->inp_ctr);
				ofTargetRVA = ofCurrRVA + (__address)dwOffset;
				pTargetAddress = pCurrAddress + dwOffset;
			}
		}/* end else */
	}/* end if */

	/*
	 * 目标地址是否已经得到处理
	 */
	pJmpTargetInst = ThisInstructionIsInDataBase(pJmpTarget, ofTargetRVA);
	if (pJmpTargetInst)
		return 0;

	/* 
	 * 如果超出保护范围则忽略
	 */
	if ((ofTargetRVA < ofProcedureRVA) || (ofTargetRVA >= (ofProcedureRVA + iSize))) {
		return 0;
	}

	/*
	 * 取得目标地址上一条指令长度,需要重新分析此篇区域
	 * 这里造成效率过慢,暂时没有好的算法处理
	 */
	ud_init(&udObjNow);
	ud_set_input_buffer(&udObjNow, pProcedure, iSize);
	ud_set_mode(&udObjNow, 32);
	ud_set_syntax(&udObjNow, UD_SYN_INTEL);
	ofNowRVA = ofProcedureRVA;//执行函数头
	while (ud_disassemble(&udObjNow)) {
		iCurrInstLen = udObjNow.inp_ctr;
		ofNowRVA += iCurrInstLen;
	
		// 确定是跳转目标地址
		if (ofNowRVA == ofTargetRVA) {
			// 如果目标是函数头,则它的上一条指令的长度确定为0,这种情况应该不会出现
			if (ofNowRVA == ofProcedureRVA) iPrevInstRandLen = 0;
			else iPrevInstRandLen = GetRandDword() % iCurrInstLen + 1;//随机取上一条指令的一段长度
			SetChaosVmpInstRecord(&(pJmpTarget->pInstList[pJmpTarget->iAddressCount]), ofTargetRVA, ofProcedureRVA, pTargetAddress, iPrevInstRandLen);
			(pJmpTarget->iAddressCount)++;//增加跳转目标地址的计数
			break;
		}
	}

	return iPrevInstRandLen;//返回跳转目标地址上一条指令的随机长度
}

/*
 * 参数:
 *  pJmpTarget:跳转指令记录结构
 *  pProcedure:函数的文件地址
 *  iSize:函数的大小
 *  ofProcedureRVA:函数的内存地址RVA
 *
 * 介绍:
 *	遍历函数并记录函数的每条跳转指令,并计算它的跳转地址并将这些信息放入到记录结构"CHAOSVMP_JMPTARGET"中,并返回当前函数的跳转目标的计数
 */
__INLINE__ __integer __INTERNAL_FUNC__ RecordAllJmpTargetInst(PCHAOSVMP_JMPTARGET pJmpTarget, __memory pProcedure, __integer iSize, __offset ofProcedureRVA) {
	__offset ofOffset = 0;
	__memory pCurrAddress = pProcedure;
	__offset ofCurrAddressRVA = ofProcedureRVA;
	__integer iInstLen = 0;
	__integer iCount = 0;
	__integer iRet = 0;
	__integer iInstIndex = 0;
	ud_t ud_obj;
	ud_init(&ud_obj);
	ud_set_input_buffer(&ud_obj, pProcedure, iSize);
	ud_set_mode(&ud_obj, 32);
	ud_set_syntax(&ud_obj, UD_SYN_INTEL);
	while (ud_disassemble(&ud_obj)) {
		iInstLen = ud_obj.inp_ctr;
		// 如果是跳转指令则进行处理
		if (VmpThisInstructionIsFlowInstruction(&ud_obj)) {
			// 记录跳转指令的基本信息,如跳转后的目标地址
			iRet = VmpRecordJmpTargetInfo(&ud_obj, pProcedure, iSize, ofProcedureRVA, ofOffset, pJmpTarget, iInstIndex);
			if (iRet) iCount++;
		}

		ofOffset += (__offset)iInstLen;
		pCurrAddress += iInstLen;
		ofCurrAddressRVA += (__address)iInstLen;
		iInstIndex++;
	}
	return iCount;
}

// 此函数供快速排序算法所用
__sinteger __INTERNAL_FUNC__ SoftJmpTargetRecordCompare(__void *pTarget, __void *pNow) {
	__offset ofTargetRVA = ((PCHAOSVMP_JMPTARGET_INST)pTarget)->ofRVA;
	__offset ofNowRVA = ((PCHAOSVMP_JMPTARGET_INST)pNow)->ofRVA;
	return (__sinteger)(ofTargetRVA - ofNowRVA);
}

/*
 * 参数:
 *	pJmpTarget:跳转目标结构
 *
 * 介绍:
 *	按住地址从小到大重新排序跳转目标结构
 */
__INLINE__ __void __INTERNAL_FUNC__ SortJmpTargetRecord(PCHAOSVMP_JMPTARGET pJmpTarget) {
	__integer iCount = pJmpTarget->iAddressCount;
	__logic_qsort__((__void *)(pJmpTarget->pInstList), iCount, sizeof(CHAOSVMP_JMPTARGET_INST), SoftJmpTargetRecordCompare);
}

/*
 * 参数:
 *	pJmpTarget:跳转目标结构
 * pInstructionList:指令结构列表
 * pProcedure:原始的函数文件地址
 * pVmpProcedure:被保护函数的文件地址
 * dwHeaderKey:头指令的KEY
 * pVmHash:HASH函数
 * pVmEncrypt:加密函数
 *
 * 介绍:
 *	加密跳转目标指令结构,并加密这个结构中的每条跳转目标指令,并返回这条指令在跳转目标结构中的索引
 */
__INLINE__ __integer __INTERNAL_FUNC__ VmpJmpTargetRecord(PCHAOSVMP_JMPTARGET pJmpTarget, PCHAOSVMP_INSTRUCTION pInstructionList, __memory pProcedure, \
														  __memory pVmpProcedure, __dword dwHeaderKey, FPVmHash pVmHash, FPVmEncrypt pVmEncrypt) {
	__integer i = 0;
	SortJmpTargetRecord(pJmpTarget);//对跳转目标地址进行排序
	for (i = 0; i < pJmpTarget->iAddressCount; i++) {
		PCHAOSVMP_JMPTARGET_INST pJmpTargetInst = &(pJmpTarget->pInstList[i]);
		__memory pIn = pJmpTargetInst->pAddress;//跳转目标指令的当前文件地址
		__memory pOut = pVmpProcedure + (__integer)(pIn - pProcedure);//跳转目标指令被虚拟化后的文件地址
		__integer iInstIndex = pJmpTargetInst->iInstIndex;//跳转目标指令的索引
		__integer iEncryptLength = pInstructionList[iInstIndex].iInstEncryptLength;//这条指令指令部分要加密的长度

		/*
		 * 如果上一条指令长度为0,则使用dwHeaderKey
		 * 如果上一条指令长度不为0,则使用上一条指令的密文作为密钥
		 */
		if (!(pJmpTargetInst->iPrevInstRandLen))
			pVmEncrypt(pIn, iEncryptLength, dwHeaderKey, pOut);
		else {//否则使用上一条指令的密文作为密钥
			__integer iPrevInstRandLen = pJmpTargetInst->iPrevInstRandLen;//从跳转目标指令结构中取出上一条指令的随机长度
			__memory pPrevInst = pOut - iPrevInstRandLen;
			__dword dwKey = pVmHash(pPrevInst, iPrevInstRandLen);
			pVmEncrypt(pOut, iEncryptLength, dwKey, pOut);//加密指令
			pVmEncrypt(&(pInstructionList[iInstIndex]), sizeof(CHAOSVMP_INSTRUCTION), dwKey, &(pInstructionList[iInstIndex]));//加密指令信息结构
		}
	}
	return i;
}

/*
 * 参数:
 *  pProcedure:要进行保护函数的文件地址
 *  iSize:要保护的长度
 *  ofProcedureRVA:要保护函数的内存RVA
 *  pVmHash:用作进行HASH的函数
 *  pVmEncrypt:用于加密指令的函数
 *  pInstRemainEncrypt:用于加密指令剩余长度的函数
 *  pOpcode1Tbl:Opcode表1
 *  pOpcode2Tbl:Opcode表2
 *  pModRmTbl:ModRm表
 *  pSibTbl:Sib表
 *  pInvokeThunkCode:原始函数头要填充的花指令
 *  iInvokeVMSize:原始函数头要填充的花指令长度
 *  dwKey:指定此函数起始KEY
 *  bHeader:此函数是否是头函数
 *	pNextKey:输出下一个要保护函数的KEY
 *	pKeyProcedure:密钥函数的指针
 *	iKeyProcedureSize:密钥函数的长度
 *	ofKeyProcedureRVA:密钥函数的RVA
 *
 * 介绍:
 *  第一条指令采用要保护函数的明文的HASH作为KEY,其余指令则采用上一条指令的HASH作为KEY,如果遇到跳转指令并且跳转的目标是保护区域内
 *  则记录跳转目标地址上一条指令的长度,如果跳转到保护函数头,则上一条指令的长度为0,如果是跳到保护区域外则直接忽略。并且跳转目标指令是以
 *  上条指令的密文为密钥进行加密的.如果VmpThisInstruction返回不为0,则将剩余数据使用另外的加密算法进行加密.最后将函数的密文作为下一个被保护
 *  函数的起始KEY,并用此密钥加密JMPTARGET结构
 */
PCHAOSVMP_PROCEDURE __API__ VmpThisProcedure(__memory pProcedure, __integer iSize, __offset ofProcedureRVA, FPVmHash pVmHash, \
											 FPVmEncrypt pVmEncrypt, FPVmInstRemainEncrypt pInstRemainEncrypt, __byte *pOpcode1Tbl, \
											 __byte *pOpcode2Tbl, __byte *pModRmTbl, __byte *pSibTbl, __memory pInvokeThunkCode, __integer iInvokeThunkCodeSize, \
											 __dword dwKey, __bool bHeader, __dword *pNextKey, \
											 __memory pKeyProcedure, __integer iKeyProcedureSize, __offset ofKeyProcedureRVA) {
	PCHAOSVMP_INSTRUCTION pInstruction = NULL;
	PCHAOSVMP_PROCEDURE pChaosVmpProcedure = __logic_new__(CHAOSVMP_PROCEDURE, 1);
	PCHAOSVMP_JMPTARGET pJmpTarget = &(pChaosVmpProcedure->JmpTargetRecord);
	__memory pVmpProcedure = __logic_new__(__byte, iSize);
	__byte OneInst[0x10] = {0};
	__byte ChangedInst[0x10] = {0};
	__offset ofOffset = 0;
	__integer iInstIndex = 0;
	ud_t ud_obj;
	ud_init(&ud_obj);
	ud_set_input_buffer(&ud_obj, pProcedure, iSize);
	ud_set_mode(&ud_obj, 32);
	ud_set_syntax(&ud_obj, UD_SYN_INTEL);

	__logic_memset__(pChaosVmpProcedure, 0, sizeof(CHAOSVMP_PROCEDURE));
	/*
	 * 第一条指令的Key,如果不由外部指定, 则通过保护函数明文得到
	 */
	if (bHeader) if (!dwKey) dwKey = pVmHash(pProcedure, iSize);//初始化KEY
	pChaosVmpProcedure->dwKey = dwKey;//记录初始化KEY
	
	/*
	 * 将此函数的跳转目标地址进行记录
	 */
	RecordAllJmpTargetInst(pJmpTarget, pProcedure, iSize, ofProcedureRVA);

	/*
	 * 分析函数
	 */
	while (ud_disassemble(&ud_obj)) {
		__integer iInstLen = ud_obj.inp_ctr;
		__memory pInstBuf = &(ud_obj.inp_sess);
		__integer iRet = 0;
		__integer iEncryptLen = 0;
		PCHAOSVMP_JMPTARGET_INST pJmpTargetInst = NULL;
		__offset ofNowInstRVA = ofProcedureRVA + (__address)ofOffset;//当前指令的地址
		__logic_memcpy__(OneInst, pInstBuf, iInstLen);//复制当前指令字节码
		// 获取被保护函数指令记录结构的指针
		pInstruction = &(pChaosVmpProcedure->ProtectInstruction[pChaosVmpProcedure->iInstCount]);

		/*
		 * 鉴别当前指令是否存在于目标地址列表内
		 * 如果存在,则忽略此次加密,但是直接编码这条指令
		 */
		pJmpTargetInst = ThisInstructionIsInDataBase(pJmpTarget, ofProcedureRVA + (__address)ofOffset);
		if (pJmpTargetInst) {
			iRet = VmpThisInstruction(&ud_obj, OneInst, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);//编码这条指令
			// 检验保护指令是否出错
			if (__VMP_INST_ERROR__(iRet)) {
				OnVmpThisInstructionError(&ud_obj, iRet, ofOffset);
				goto _error;
			}/* end if */
			__logic_memcpy__(ChangedInst, OneInst, iInstLen);//ChangedInst与OneInst同值
			// 如果iRet不为0则表示此条指令带有"偏移+立即数"部分,这部分别当作是额外数据进行分段加密,iRet正是这部分的长度
			if (iRet) {
				__integer iRemainDataLocal = iInstLen - iRet;//指令总长度 - 数据部分长度 = 数据部分的偏移(纯指令部分的长度)
				__memory pRemainData = OneInst + iRemainDataLocal;//得到指令数据部分的指针
				__dword dwRemainDataKey = 0;
				dwRemainDataKey = pVmHash(ChangedInst, iRemainDataLocal);//生成剩余数据KEY,使用指令部分明文作为密钥
				pInstRemainEncrypt(pRemainData, iRet, dwRemainDataKey, pRemainData);//加密剩余数据
			}

			// 这里只做编码与复制但是不对指令进行加密,等正常流程指令加密完毕后,在VmpJmpTargetRecord中进行加密
			__logic_memcpy__(pVmpProcedure + (__integer)ofOffset, OneInst, iInstLen);//写入,这里的OneInst已经经过转码值与ChangedInst相同
			iEncryptLen = iInstLen - iRet;//得到纯指令的长度
			pInstruction->iInstEncryptLength = iEncryptLen;//记录纯指令加密的长度
			pJmpTargetInst->iInstIndex = iInstIndex;//指令的索引
			(__integer)ofOffset += iInstLen;//增加偏移
			(pChaosVmpProcedure->iInstCount)++;//指令计数增加
			dwKey = pVmHash(ChangedInst, iEncryptLen);//生成下条指令的密钥,使用当前指令指令部分的明文作为KEY
			iInstIndex++;//指令的索引增加
			// 到这里直接进入下一轮循环,不在往下执行
			continue;
		}

		/*
		 * 非跳转目标地址则保护这条指令
		 */
		iRet = VmpThisInstruction(&ud_obj, OneInst, pOpcode1Tbl, pOpcode2Tbl, pModRmTbl, pSibTbl);//编码这条指令
		// 检验保护指令是否出错
		if (__VMP_INST_ERROR__(iRet)) {
			OnVmpThisInstructionError(&ud_obj, iRet, ofOffset);
			goto _error;
		}/* end if */
		__logic_memcpy__(ChangedInst, OneInst, iInstLen);
		// 如果iRet不为0则表示此条指令带有"偏移+立即数"部分,这部分别当作是额外数据进行分段加密,iRet正是这部分的长度
		if (iRet) {
			__integer iRemainDataLocal = iInstLen - iRet;//指令总长度 - 数据部分长度 = 数据部分的偏移(纯指令部分的长度)
			__memory pRemainData = OneInst + iRemainDataLocal;//得到指令数据部分的指针
			__dword dwRemainDataKey = 0;
			dwRemainDataKey = pVmHash(ChangedInst, iRemainDataLocal);//生成剩余数据KEY,使用指令部分明文作为密钥
			pVmEncrypt(OneInst, iRemainDataLocal, dwKey, OneInst);//加密这条指令
			pInstRemainEncrypt(pRemainData, iRet, dwRemainDataKey, pRemainData);//加密剩余数据
		} else //为零直接加密这条指令
			pVmEncrypt(OneInst, iInstLen, dwKey, OneInst);//加密这条指令

		__logic_memcpy__(pVmpProcedure + (__integer)ofOffset, OneInst, iInstLen);//写入,这里的OneInst已经经过转码值与ChangedInst相同
		iEncryptLen = iInstLen - iRet;//得到纯指令的长度
		pInstruction->iInstEncryptLength = iEncryptLen;//记录纯指令加密的长度
		pVmEncrypt((__memory)pInstruction, sizeof(CHAOSVMP_INSTRUCTION), dwKey, (__memory)pInstruction);//加密这条指令对于的记录结构
		dwKey = pVmHash(ChangedInst, iEncryptLen);//生成下条指令的密钥,使用当前指令的指令部分明文作为KEY
		(__integer)ofOffset += iInstLen;//增加偏移
		(pChaosVmpProcedure->iInstCount)++;//指令计数增加
		iInstIndex++;
	}
	
	/*
	 * 当所有正常指令被编码加密后,则开始对跳转目标所在的指令进行加密
	 */
	VmpJmpTargetRecord(pJmpTarget, &(pChaosVmpProcedure->ProtectInstruction), pProcedure, pVmpProcedure, pChaosVmpProcedure->dwKey, pVmHash, pVmEncrypt);

	/*
	 * 计算此函数的密体KEY,作为下一个函数的起始KEY
	 * 此密钥也作为加密JMPTARGET结构的密钥
	 */
	*pNextKey = pVmHash(pVmpProcedure, iSize);
	pVmEncrypt((__memory)&(pChaosVmpProcedure->JmpTargetRecord), sizeof(CHAOSVMP_JMPTARGET), \
				*pNextKey, (__memory)&(pChaosVmpProcedure->JmpTargetRecord));
	
	/*
	 * 这里产生随机花指令填充到原始目标函数内,这里这篇区域在虚拟机加载起来后
	 * 会再一次进行真正的调用头填充
	 */
	__logic_memset__(pProcedure, 0, iSize);
	if (pInvokeThunkCode && iInvokeThunkCodeSize) __logic_memcpy__(pProcedure, pInvokeThunkCode, iInvokeThunkCodeSize);//复制调用头

	/*
	 * 对转码后的函数进行加密
	 */
	{
		__dword dwProcKey = 0;
		if (iKeyProcedureSize == 0) {
			dwProcKey = GetRandDword();
			pChaosVmpProcedure->dwProcKey = dwProcKey;
			pChaosVmpProcedure->ofKeyRVA = 0;
			pChaosVmpProcedure->iKeySize = 0;
		} else {
			dwProcKey = __GenProcedureKey__(pKeyProcedure, iKeyProcedureSize);
			pChaosVmpProcedure->dwProcKey = dwProcKey;
			pChaosVmpProcedure->ofKeyRVA = ofKeyProcedureRVA;
			pChaosVmpProcedure->iKeySize = iKeyProcedureSize;
		}
		XorArray(dwProcKey, pVmpProcedure, pVmpProcedure, iSize);
	}

	/*
	 * 复制被保护的函数
	 */
	pChaosVmpProcedure->pVmpProcedure = pVmpProcedure;//保护后的函数
	pChaosVmpProcedure->iSize = iSize;//大小,永远指向原始大小
	__logic_memcpy__(&(pChaosVmpProcedure->JmpTargetRecord), pJmpTarget, sizeof(CHAOSVMP_JMPTARGET));//复制跳转目标记录

	/*
	 * 设置头标记,并设置函数内存地址
	 */
	pChaosVmpProcedure->bHeader = bHeader;
	pChaosVmpProcedure->ofProcedureMemoryAddress = ofProcedureRVA;
	return pChaosVmpProcedure;

_error:
	if (pVmpProcedure) __logic_delete__(pVmpProcedure);
	if (pChaosVmpProcedure) __logic_delete__(pChaosVmpProcedure);
	return NULL;
}
