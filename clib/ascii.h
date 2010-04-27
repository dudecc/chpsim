/* ascii.h: ascii control characters 		*/

#ifndef ASCII_H
#define ASCII_H


#define ASCII_NUL '\000' /* null 			*/
#define ASCII_SOH '\001' /* start of heading 		*/
#define ASCII_STX '\002' /* start of text 		*/
#define ASCII_ETX '\003' /* end of text 		*/
#define ASCII_EOT '\004' /* end of tape 		*/
#define ASCII_ENQ '\005' /* enquiry 			*/
#define ASCII_ACK '\006' /* acknowledge 		*/
#define ASCII_BEL '\007' /* bell, alert 		*/

#define ASCII_BS '\010'  /* backspace 		*/
#define ASCII_HT '\011'  /* horizontal tab 		*/
#define ASCII_LF '\012'  /* line feed 		*/
#define ASCII_VT '\013'  /* vertical tab 		*/
#define ASCII_FF '\014'  /* form feed 		*/
#define ASCII_CR '\015'  /* carriage return 		*/
#define ASCII_SO '\016'  /* shift out 		*/
#define ASCII_SI '\017'  /* shift in 			*/

#define ASCII_DLE '\020' /* data link escape 		*/
#define ASCII_DC1 '\021' /* device control 1 		*/
#define ASCII_DC2 '\022' /* device control 2 		*/
#define ASCII_DC3 '\023' /* device control 3 		*/
#define ASCII_DC4 '\024' /* device control 4 		*/
#define ASCII_NAK '\025' /* negative acknowledge 	*/
#define ASCII_SYN '\026' /* synchronize 		*/
#define ASCII_ETB '\027' /* end of transmitted block 	*/

#define ASCII_CAN '\030' /* cancel 			*/
#define ASCII_EM '\031'  /* end of medium 		*/
#define ASCII_SUB '\032' /* substitute 		*/
#define ASCII_ESC '\033' /* escape 			*/
#define ASCII_FS '\034'  /* file separator 		*/
#define ASCII_GS '\035'  /* group separator 		*/
#define ASCII_RS '\036'  /* record separator 		*/
#define ASCII_US '\037'  /* unit separator 		*/

#define ASCII_SP '\040'  /* space, ' ' 		*/

#define ASCII_DEL '\177' /* delete 			*/

/* alternate names */

#define ASCII_TAB '\011' /* HT 			*/
#define ASCII_XON '\021' /* DC1 (ctrl-Q)		*/
#define ASCII_XOFF '\023' /*DC3 (ctrl-S)		*/

/* ctrl-A etc: */
#define ASCII_CTRL(X) ((X)&31) /* arg is '@', 'A'..'Z', 'a'..'z' */

/* convert to ascii; this is relevant if you really must have ascii
   codes on a machine that does not use ascii.
*/
#define ASCII(X) ((X)&127)

#endif
