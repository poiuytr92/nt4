/* VS_WORK.C 18/04/95 13.29.52 */
VW_ENTRYSC SHORT VW_ENTRYMOD VwStreamOpenFunc (SOFILE fp, SHORT wFileId, BYTE
	 VWPTR *pFileName, SOFILTERINFO VWPTR *pFilterInfo, HPROC hProc);
VW_ENTRYSC SHORT VW_ENTRYMOD VwStreamSectionFunc (SOFILE fp, HPROC hProc);
VW_LOCALSC VOID VW_LOCALMOD load_para_fkp (HPROC hProc);
VW_LOCALSC VOID VW_LOCALMOD load_char_fkp (HPROC hProc);
VW_LOCALSC SHORT VW_LOCALMOD mget_short (BYTE VWPTR *pointer, HPROC hProc);
VW_LOCALSC DWORD VW_LOCALMOD fget_long (HPROC hProc);
VW_LOCALSC WORD VW_LOCALMOD fget_short (HPROC hProc);
VW_LOCALSC SHORT VW_LOCALMOD set_next_limit (HPROC hProc);
VW_LOCALSC VOID VW_LOCALMOD SetSymbolAttributes (REGISTER CHP VWPTR *chp1,
	 REGISTER CHP VWPTR *chp2, HPROC hProc);
VW_LOCALSC VOID VW_LOCALMOD pap_init (REGISTER PAP VWPTR *pap, HPROC hProc);
VW_LOCALSC VOID VW_LOCALMOD chp_init (REGISTER CHP VWPTR *chp, HPROC hProc);
VW_LOCALSC VOID VW_LOCALMOD chp_processor (HPROC hProc);
VW_LOCALSC VOID VW_LOCALMOD pap_processor (HPROC hProc);
VW_LOCALSC SHORT VW_LOCALMOD TokenHandler (DWORD cpCurrToken, HPROC hProc);
VW_LOCALSC SHORT VW_LOCALMOD PicHandler (HPROC hProc);
VW_ENTRYSC SHORT VW_ENTRYMOD VwStreamReadFunc (SOFILE fp, HPROC hProc);
VW_ENTRYSC VOID VW_ENTRYMOD VwStreamCloseFunc (SOFILE hFile, HPROC hProc);
VW_ENTRYSC SHORT VW_ENTRYMOD VwStreamTellFunc (SOFILE hFile, HPROC hProc);
VW_ENTRYSC SHORT VW_ENTRYMOD VwStreamSeekFunc (SOFILE hFile, HPROC hProc);
