#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <inttypes.h>
#include <getopt.h>
#include <limits.h>
#include <linux/fs.h>
#include <stdbool.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <blkid.h>

#include "bcache.h"

#define __KERNEL__
#include <linux/bcache-ioctl.h>
#undef __KERNEL__

const char * const cache_state[] = {
	"active",
	"ro",
	"failed",
	"spare",
	NULL
};

const char * const replacement_policies[] = {
	"lru",
	"fifo",
	"random",
	NULL
};

const char * const csum_types[] = {
	"none",
	"crc32c",
	"crc64",
	NULL
};

const char * const bdev_cache_mode[] = {
	"writethrough",
	"writeback",
	"writearound",
	"none",
	NULL
};

const char * const bdev_state[] = {
	"detached",
	"clean",
	"dirty",
	"inconsistent",
	NULL
};

char *skip_spaces(const char *str)
{
	while (isspace(*str))
		++str;
	return (char *)str;
}

char *strim(char *s)
{
	size_t size;
	char *end;

	s = skip_spaces(s);
	size = strlen(s);
	if (!size)
		return s;

	end = s + size - 1;
	while (end >= s && isspace(*end))
		end--;
	*(end + 1) = '\0';

	return s;
}

ssize_t read_string_list(const char *buf, const char * const list[])
{
	size_t i;
	char *s, *d = strdup(buf);
	if (!d)
		return -ENOMEM;

	s = strim(d);

	for (i = 0; list[i]; i++)
		if (!strcmp(list[i], s))
			break;

	free(d);

	if (!list[i])
		return -EINVAL;

	return i;
}

ssize_t read_string_list_or_die(const char *opt, const char * const list[],
				const char *msg)
{
	ssize_t v = read_string_list(opt, list);
	if (v < 0) {
		fprintf(stderr, "Bad %s %s\n", msg, opt);
		exit(EXIT_FAILURE);

	}

	return v;
}

void print_string_list(const char * const list[], size_t selected)
{
	size_t i;

	for (i = 0; list[i]; i++) {
		if (i)
			putchar(' ');
		printf(i == selected ? "[%s] ": "%s", list[i]);
	}
}

/*
 * This is the CRC-32C table
 * Generated with:
 * width = 32 bits
 * poly = 0x1EDC6F41
 * reflect input bytes = true
 * reflect output bytes = true
 */

static const u32 crc32c_table[256] = {
	0x00000000L, 0xF26B8303L, 0xE13B70F7L, 0x1350F3F4L,
	0xC79A971FL, 0x35F1141CL, 0x26A1E7E8L, 0xD4CA64EBL,
	0x8AD958CFL, 0x78B2DBCCL, 0x6BE22838L, 0x9989AB3BL,
	0x4D43CFD0L, 0xBF284CD3L, 0xAC78BF27L, 0x5E133C24L,
	0x105EC76FL, 0xE235446CL, 0xF165B798L, 0x030E349BL,
	0xD7C45070L, 0x25AFD373L, 0x36FF2087L, 0xC494A384L,
	0x9A879FA0L, 0x68EC1CA3L, 0x7BBCEF57L, 0x89D76C54L,
	0x5D1D08BFL, 0xAF768BBCL, 0xBC267848L, 0x4E4DFB4BL,
	0x20BD8EDEL, 0xD2D60DDDL, 0xC186FE29L, 0x33ED7D2AL,
	0xE72719C1L, 0x154C9AC2L, 0x061C6936L, 0xF477EA35L,
	0xAA64D611L, 0x580F5512L, 0x4B5FA6E6L, 0xB93425E5L,
	0x6DFE410EL, 0x9F95C20DL, 0x8CC531F9L, 0x7EAEB2FAL,
	0x30E349B1L, 0xC288CAB2L, 0xD1D83946L, 0x23B3BA45L,
	0xF779DEAEL, 0x05125DADL, 0x1642AE59L, 0xE4292D5AL,
	0xBA3A117EL, 0x4851927DL, 0x5B016189L, 0xA96AE28AL,
	0x7DA08661L, 0x8FCB0562L, 0x9C9BF696L, 0x6EF07595L,
	0x417B1DBCL, 0xB3109EBFL, 0xA0406D4BL, 0x522BEE48L,
	0x86E18AA3L, 0x748A09A0L, 0x67DAFA54L, 0x95B17957L,
	0xCBA24573L, 0x39C9C670L, 0x2A993584L, 0xD8F2B687L,
	0x0C38D26CL, 0xFE53516FL, 0xED03A29BL, 0x1F682198L,
	0x5125DAD3L, 0xA34E59D0L, 0xB01EAA24L, 0x42752927L,
	0x96BF4DCCL, 0x64D4CECFL, 0x77843D3BL, 0x85EFBE38L,
	0xDBFC821CL, 0x2997011FL, 0x3AC7F2EBL, 0xC8AC71E8L,
	0x1C661503L, 0xEE0D9600L, 0xFD5D65F4L, 0x0F36E6F7L,
	0x61C69362L, 0x93AD1061L, 0x80FDE395L, 0x72966096L,
	0xA65C047DL, 0x5437877EL, 0x4767748AL, 0xB50CF789L,
	0xEB1FCBADL, 0x197448AEL, 0x0A24BB5AL, 0xF84F3859L,
	0x2C855CB2L, 0xDEEEDFB1L, 0xCDBE2C45L, 0x3FD5AF46L,
	0x7198540DL, 0x83F3D70EL, 0x90A324FAL, 0x62C8A7F9L,
	0xB602C312L, 0x44694011L, 0x5739B3E5L, 0xA55230E6L,
	0xFB410CC2L, 0x092A8FC1L, 0x1A7A7C35L, 0xE811FF36L,
	0x3CDB9BDDL, 0xCEB018DEL, 0xDDE0EB2AL, 0x2F8B6829L,
	0x82F63B78L, 0x709DB87BL, 0x63CD4B8FL, 0x91A6C88CL,
	0x456CAC67L, 0xB7072F64L, 0xA457DC90L, 0x563C5F93L,
	0x082F63B7L, 0xFA44E0B4L, 0xE9141340L, 0x1B7F9043L,
	0xCFB5F4A8L, 0x3DDE77ABL, 0x2E8E845FL, 0xDCE5075CL,
	0x92A8FC17L, 0x60C37F14L, 0x73938CE0L, 0x81F80FE3L,
	0x55326B08L, 0xA759E80BL, 0xB4091BFFL, 0x466298FCL,
	0x1871A4D8L, 0xEA1A27DBL, 0xF94AD42FL, 0x0B21572CL,
	0xDFEB33C7L, 0x2D80B0C4L, 0x3ED04330L, 0xCCBBC033L,
	0xA24BB5A6L, 0x502036A5L, 0x4370C551L, 0xB11B4652L,
	0x65D122B9L, 0x97BAA1BAL, 0x84EA524EL, 0x7681D14DL,
	0x2892ED69L, 0xDAF96E6AL, 0xC9A99D9EL, 0x3BC21E9DL,
	0xEF087A76L, 0x1D63F975L, 0x0E330A81L, 0xFC588982L,
	0xB21572C9L, 0x407EF1CAL, 0x532E023EL, 0xA145813DL,
	0x758FE5D6L, 0x87E466D5L, 0x94B49521L, 0x66DF1622L,
	0x38CC2A06L, 0xCAA7A905L, 0xD9F75AF1L, 0x2B9CD9F2L,
	0xFF56BD19L, 0x0D3D3E1AL, 0x1E6DCDEEL, 0xEC064EEDL,
	0xC38D26C4L, 0x31E6A5C7L, 0x22B65633L, 0xD0DDD530L,
	0x0417B1DBL, 0xF67C32D8L, 0xE52CC12CL, 0x1747422FL,
	0x49547E0BL, 0xBB3FFD08L, 0xA86F0EFCL, 0x5A048DFFL,
	0x8ECEE914L, 0x7CA56A17L, 0x6FF599E3L, 0x9D9E1AE0L,
	0xD3D3E1ABL, 0x21B862A8L, 0x32E8915CL, 0xC083125FL,
	0x144976B4L, 0xE622F5B7L, 0xF5720643L, 0x07198540L,
	0x590AB964L, 0xAB613A67L, 0xB831C993L, 0x4A5A4A90L,
	0x9E902E7BL, 0x6CFBAD78L, 0x7FAB5E8CL, 0x8DC0DD8FL,
	0xE330A81AL, 0x115B2B19L, 0x020BD8EDL, 0xF0605BEEL,
	0x24AA3F05L, 0xD6C1BC06L, 0xC5914FF2L, 0x37FACCF1L,
	0x69E9F0D5L, 0x9B8273D6L, 0x88D28022L, 0x7AB90321L,
	0xAE7367CAL, 0x5C18E4C9L, 0x4F48173DL, 0xBD23943EL,
	0xF36E6F75L, 0x0105EC76L, 0x12551F82L, 0xE03E9C81L,
	0x34F4F86AL, 0xC69F7B69L, 0xD5CF889DL, 0x27A40B9EL,
	0x79B737BAL, 0x8BDCB4B9L, 0x988C474DL, 0x6AE7C44EL,
	0xBE2DA0A5L, 0x4C4623A6L, 0x5F16D052L, 0xAD7D5351L
};

/*
 * Steps through buffer one byte at at time, calculates reflected
 * crc using table.
 */

static u32 crc32c(u32 crc, unsigned char const *data, size_t length)
{
	while (length--)
		crc =
		    crc32c_table[(crc ^ *data++) & 0xFFL] ^ (crc >> 8);
	return crc;
}

/*
 * Portions Copyright (c) 1996-2001, PostgreSQL Global Development Group (Any
 * use permitted, subject to terms of PostgreSQL license; see.)

 * If we have a 64-bit integer type, then a 64-bit CRC looks just like the
 * usual sort of implementation. (See Ross Williams' excellent introduction
 * A PAINLESS GUIDE TO CRC ERROR DETECTION ALGORITHMS, available from
 * ftp://ftp.rocksoft.com/papers/crc_v3.txt or several other net sites.)
 * If we have no working 64-bit type, then fake it with two 32-bit registers.
 *
 * The present implementation is a normal (not "reflected", in Williams'
 * terms) 64-bit CRC, using initial all-ones register contents and a final
 * bit inversion. The chosen polynomial is borrowed from the DLT1 spec
 * (ECMA-182, available from http://www.ecma.ch/ecma1/STAND/ECMA-182.HTM):
 *
 * x^64 + x^62 + x^57 + x^55 + x^54 + x^53 + x^52 + x^47 + x^46 + x^45 +
 * x^40 + x^39 + x^38 + x^37 + x^35 + x^33 + x^32 + x^31 + x^29 + x^27 +
 * x^24 + x^23 + x^22 + x^21 + x^19 + x^17 + x^13 + x^12 + x^10 + x^9 +
 * x^7 + x^4 + x + 1
*/

static const uint64_t crc_table[256] = {
	0x0000000000000000ULL, 0x42F0E1EBA9EA3693ULL, 0x85E1C3D753D46D26ULL,
	0xC711223CFA3E5BB5ULL, 0x493366450E42ECDFULL, 0x0BC387AEA7A8DA4CULL,
	0xCCD2A5925D9681F9ULL, 0x8E224479F47CB76AULL, 0x9266CC8A1C85D9BEULL,
	0xD0962D61B56FEF2DULL, 0x17870F5D4F51B498ULL, 0x5577EEB6E6BB820BULL,
	0xDB55AACF12C73561ULL, 0x99A54B24BB2D03F2ULL, 0x5EB4691841135847ULL,
	0x1C4488F3E8F96ED4ULL, 0x663D78FF90E185EFULL, 0x24CD9914390BB37CULL,
	0xE3DCBB28C335E8C9ULL, 0xA12C5AC36ADFDE5AULL, 0x2F0E1EBA9EA36930ULL,
	0x6DFEFF5137495FA3ULL, 0xAAEFDD6DCD770416ULL, 0xE81F3C86649D3285ULL,
	0xF45BB4758C645C51ULL, 0xB6AB559E258E6AC2ULL, 0x71BA77A2DFB03177ULL,
	0x334A9649765A07E4ULL, 0xBD68D2308226B08EULL, 0xFF9833DB2BCC861DULL,
	0x388911E7D1F2DDA8ULL, 0x7A79F00C7818EB3BULL, 0xCC7AF1FF21C30BDEULL,
	0x8E8A101488293D4DULL, 0x499B3228721766F8ULL, 0x0B6BD3C3DBFD506BULL,
	0x854997BA2F81E701ULL, 0xC7B97651866BD192ULL, 0x00A8546D7C558A27ULL,
	0x4258B586D5BFBCB4ULL, 0x5E1C3D753D46D260ULL, 0x1CECDC9E94ACE4F3ULL,
	0xDBFDFEA26E92BF46ULL, 0x990D1F49C77889D5ULL, 0x172F5B3033043EBFULL,
	0x55DFBADB9AEE082CULL, 0x92CE98E760D05399ULL, 0xD03E790CC93A650AULL,
	0xAA478900B1228E31ULL, 0xE8B768EB18C8B8A2ULL, 0x2FA64AD7E2F6E317ULL,
	0x6D56AB3C4B1CD584ULL, 0xE374EF45BF6062EEULL, 0xA1840EAE168A547DULL,
	0x66952C92ECB40FC8ULL, 0x2465CD79455E395BULL, 0x3821458AADA7578FULL,
	0x7AD1A461044D611CULL, 0xBDC0865DFE733AA9ULL, 0xFF3067B657990C3AULL,
	0x711223CFA3E5BB50ULL, 0x33E2C2240A0F8DC3ULL, 0xF4F3E018F031D676ULL,
	0xB60301F359DBE0E5ULL, 0xDA050215EA6C212FULL, 0x98F5E3FE438617BCULL,
	0x5FE4C1C2B9B84C09ULL, 0x1D14202910527A9AULL, 0x93366450E42ECDF0ULL,
	0xD1C685BB4DC4FB63ULL, 0x16D7A787B7FAA0D6ULL, 0x5427466C1E109645ULL,
	0x4863CE9FF6E9F891ULL, 0x0A932F745F03CE02ULL, 0xCD820D48A53D95B7ULL,
	0x8F72ECA30CD7A324ULL, 0x0150A8DAF8AB144EULL, 0x43A04931514122DDULL,
	0x84B16B0DAB7F7968ULL, 0xC6418AE602954FFBULL, 0xBC387AEA7A8DA4C0ULL,
	0xFEC89B01D3679253ULL, 0x39D9B93D2959C9E6ULL, 0x7B2958D680B3FF75ULL,
	0xF50B1CAF74CF481FULL, 0xB7FBFD44DD257E8CULL, 0x70EADF78271B2539ULL,
	0x321A3E938EF113AAULL, 0x2E5EB66066087D7EULL, 0x6CAE578BCFE24BEDULL,
	0xABBF75B735DC1058ULL, 0xE94F945C9C3626CBULL, 0x676DD025684A91A1ULL,
	0x259D31CEC1A0A732ULL, 0xE28C13F23B9EFC87ULL, 0xA07CF2199274CA14ULL,
	0x167FF3EACBAF2AF1ULL, 0x548F120162451C62ULL, 0x939E303D987B47D7ULL,
	0xD16ED1D631917144ULL, 0x5F4C95AFC5EDC62EULL, 0x1DBC74446C07F0BDULL,
	0xDAAD56789639AB08ULL, 0x985DB7933FD39D9BULL, 0x84193F60D72AF34FULL,
	0xC6E9DE8B7EC0C5DCULL, 0x01F8FCB784FE9E69ULL, 0x43081D5C2D14A8FAULL,
	0xCD2A5925D9681F90ULL, 0x8FDAB8CE70822903ULL, 0x48CB9AF28ABC72B6ULL,
	0x0A3B7B1923564425ULL, 0x70428B155B4EAF1EULL, 0x32B26AFEF2A4998DULL,
	0xF5A348C2089AC238ULL, 0xB753A929A170F4ABULL, 0x3971ED50550C43C1ULL,
	0x7B810CBBFCE67552ULL, 0xBC902E8706D82EE7ULL, 0xFE60CF6CAF321874ULL,
	0xE224479F47CB76A0ULL, 0xA0D4A674EE214033ULL, 0x67C58448141F1B86ULL,
	0x253565A3BDF52D15ULL, 0xAB1721DA49899A7FULL, 0xE9E7C031E063ACECULL,
	0x2EF6E20D1A5DF759ULL, 0x6C0603E6B3B7C1CAULL, 0xF6FAE5C07D3274CDULL,
	0xB40A042BD4D8425EULL, 0x731B26172EE619EBULL, 0x31EBC7FC870C2F78ULL,
	0xBFC9838573709812ULL, 0xFD39626EDA9AAE81ULL, 0x3A28405220A4F534ULL,
	0x78D8A1B9894EC3A7ULL, 0x649C294A61B7AD73ULL, 0x266CC8A1C85D9BE0ULL,
	0xE17DEA9D3263C055ULL, 0xA38D0B769B89F6C6ULL, 0x2DAF4F0F6FF541ACULL,
	0x6F5FAEE4C61F773FULL, 0xA84E8CD83C212C8AULL, 0xEABE6D3395CB1A19ULL,
	0x90C79D3FEDD3F122ULL, 0xD2377CD44439C7B1ULL, 0x15265EE8BE079C04ULL,
	0x57D6BF0317EDAA97ULL, 0xD9F4FB7AE3911DFDULL, 0x9B041A914A7B2B6EULL,
	0x5C1538ADB04570DBULL, 0x1EE5D94619AF4648ULL, 0x02A151B5F156289CULL,
	0x4051B05E58BC1E0FULL, 0x87409262A28245BAULL, 0xC5B073890B687329ULL,
	0x4B9237F0FF14C443ULL, 0x0962D61B56FEF2D0ULL, 0xCE73F427ACC0A965ULL,
	0x8C8315CC052A9FF6ULL, 0x3A80143F5CF17F13ULL, 0x7870F5D4F51B4980ULL,
	0xBF61D7E80F251235ULL, 0xFD913603A6CF24A6ULL, 0x73B3727A52B393CCULL,
	0x31439391FB59A55FULL, 0xF652B1AD0167FEEAULL, 0xB4A25046A88DC879ULL,
	0xA8E6D8B54074A6ADULL, 0xEA16395EE99E903EULL, 0x2D071B6213A0CB8BULL,
	0x6FF7FA89BA4AFD18ULL, 0xE1D5BEF04E364A72ULL, 0xA3255F1BE7DC7CE1ULL,
	0x64347D271DE22754ULL, 0x26C49CCCB40811C7ULL, 0x5CBD6CC0CC10FAFCULL,
	0x1E4D8D2B65FACC6FULL, 0xD95CAF179FC497DAULL, 0x9BAC4EFC362EA149ULL,
	0x158E0A85C2521623ULL, 0x577EEB6E6BB820B0ULL, 0x906FC95291867B05ULL,
	0xD29F28B9386C4D96ULL, 0xCEDBA04AD0952342ULL, 0x8C2B41A1797F15D1ULL,
	0x4B3A639D83414E64ULL, 0x09CA82762AAB78F7ULL, 0x87E8C60FDED7CF9DULL,
	0xC51827E4773DF90EULL, 0x020905D88D03A2BBULL, 0x40F9E43324E99428ULL,
	0x2CFFE7D5975E55E2ULL, 0x6E0F063E3EB46371ULL, 0xA91E2402C48A38C4ULL,
	0xEBEEC5E96D600E57ULL, 0x65CC8190991CB93DULL, 0x273C607B30F68FAEULL,
	0xE02D4247CAC8D41BULL, 0xA2DDA3AC6322E288ULL, 0xBE992B5F8BDB8C5CULL,
	0xFC69CAB42231BACFULL, 0x3B78E888D80FE17AULL, 0x7988096371E5D7E9ULL,
	0xF7AA4D1A85996083ULL, 0xB55AACF12C735610ULL, 0x724B8ECDD64D0DA5ULL,
	0x30BB6F267FA73B36ULL, 0x4AC29F2A07BFD00DULL, 0x08327EC1AE55E69EULL,
	0xCF235CFD546BBD2BULL, 0x8DD3BD16FD818BB8ULL, 0x03F1F96F09FD3CD2ULL,
	0x41011884A0170A41ULL, 0x86103AB85A2951F4ULL, 0xC4E0DB53F3C36767ULL,
	0xD8A453A01B3A09B3ULL, 0x9A54B24BB2D03F20ULL, 0x5D45907748EE6495ULL,
	0x1FB5719CE1045206ULL, 0x919735E51578E56CULL, 0xD367D40EBC92D3FFULL,
	0x1476F63246AC884AULL, 0x568617D9EF46BED9ULL, 0xE085162AB69D5E3CULL,
	0xA275F7C11F7768AFULL, 0x6564D5FDE549331AULL, 0x279434164CA30589ULL,
	0xA9B6706FB8DFB2E3ULL, 0xEB46918411358470ULL, 0x2C57B3B8EB0BDFC5ULL,
	0x6EA7525342E1E956ULL, 0x72E3DAA0AA188782ULL, 0x30133B4B03F2B111ULL,
	0xF7021977F9CCEAA4ULL, 0xB5F2F89C5026DC37ULL, 0x3BD0BCE5A45A6B5DULL,
	0x79205D0E0DB05DCEULL, 0xBE317F32F78E067BULL, 0xFCC19ED95E6430E8ULL,
	0x86B86ED5267CDBD3ULL, 0xC4488F3E8F96ED40ULL, 0x0359AD0275A8B6F5ULL,
	0x41A94CE9DC428066ULL, 0xCF8B0890283E370CULL, 0x8D7BE97B81D4019FULL,
	0x4A6ACB477BEA5A2AULL, 0x089A2AACD2006CB9ULL, 0x14DEA25F3AF9026DULL,
	0x562E43B4931334FEULL, 0x913F6188692D6F4BULL, 0xD3CF8063C0C759D8ULL,
	0x5DEDC41A34BBEEB2ULL, 0x1F1D25F19D51D821ULL, 0xD80C07CD676F8394ULL,
	0x9AFCE626CE85B507ULL
};

static uint64_t bch_crc64_update(uint64_t crc, const void *_data, size_t len)
{
	const unsigned char *data = _data;

	while (len--) {
		int i = ((int) (crc >> 56) ^ *data++) & 0xFF;
		crc = crc_table[i] ^ (crc << 8);
	}

	return crc;
}

static uint64_t bch_checksum_update(unsigned type, uint64_t crc, const void *data, size_t len)
{
	switch (type) {
	case BCH_CSUM_NONE:
		return 0;
	case BCH_CSUM_CRC32C:
		return crc32c(crc, data, len);
	case BCH_CSUM_CRC64:
		return bch_crc64_update(crc, data, len);
	default:
		fprintf(stderr, "Unknown checksum type %u\n", type);
		exit(EXIT_FAILURE);
	}
}

uint64_t bch_checksum(unsigned type, const void *data, size_t len)
{
	uint64_t crc = 0xffffffffffffffffULL;

	crc = bch_checksum_update(type, crc, data, len);

	return crc ^ 0xffffffffffffffffULL;
}

uint64_t getblocks(int fd)
{
	uint64_t ret;
	struct stat statbuf;
	if (fstat(fd, &statbuf)) {
		perror("getblocks: stat error\n");
		exit(EXIT_FAILURE);
	}
	ret = statbuf.st_size / 512;
	if (S_ISBLK(statbuf.st_mode))
		if (ioctl(fd, BLKGETSIZE, &ret)) {
			perror("ioctl error getting blksize");
			exit(EXIT_FAILURE);
		}
	return ret;
}

uint64_t hatoi(const char *s)
{
	char *e;
	long long i = strtoll(s, &e, 10);
	switch (*e) {
		case 't':
		case 'T':
			i *= 1024;
		case 'g':
		case 'G':
			i *= 1024;
		case 'm':
		case 'M':
			i *= 1024;
		case 'k':
		case 'K':
			i *= 1024;
	}
	return i;
}

unsigned hatoi_validate(const char *s, const char *msg)
{
	uint64_t v = hatoi(s);

	if (v & (v - 1)) {
		fprintf(stderr, "%s must be a power of two\n", msg);
		exit(EXIT_FAILURE);
	}

	v /= 512;

	if (v > USHRT_MAX) {
		fprintf(stderr, "%s too large\n", msg);
		exit(EXIT_FAILURE);
	}

	if (!v) {
		fprintf(stderr, "%s too small\n", msg);
		exit(EXIT_FAILURE);
	}

	return v;
}

static void do_write_sb(int fd, struct cache_sb *sb)
{
	char zeroes[SB_START] = {0};
	size_t bytes = ((void *) bset_bkey_last(sb)) - (void *) sb;

	/* Zero start of disk */
	if (pwrite(fd, zeroes, SB_START, 0) != SB_START) {
		perror("write error trying to zero start of disk\n");
		exit(EXIT_FAILURE);
	}
	/* Write superblock */
	if (pwrite(fd, sb, bytes, SB_START) != bytes) {
		perror("write error trying to write superblock\n");
		exit(EXIT_FAILURE);
	}

	fsync(fd);
	close(fd);
}

void write_backingdev_sb(int fd, unsigned block_size, unsigned *bucket_sizes,
				bool writeback, uint64_t data_offset,
				const char *label,
				uuid_le set_uuid)
{
	char uuid_str[40], set_uuid_str[40];
	struct cache_sb sb;

	memset(&sb, 0, sizeof(struct cache_sb));

	sb.offset	= SB_SECTOR;
	sb.version	= BCACHE_SB_VERSION_BDEV;
	sb.magic	= BCACHE_MAGIC;
	uuid_generate(sb.uuid.b);
	sb.set_uuid	= set_uuid;
	sb.bucket_size	= bucket_sizes[0];
	sb.block_size	= block_size;

	uuid_unparse(sb.uuid.b, uuid_str);
	uuid_unparse(sb.set_uuid.b, set_uuid_str);
	if (label)
		memcpy(sb.label, label, SB_LABEL_SIZE);

	SET_BDEV_CACHE_MODE(&sb, writeback
			    ? CACHE_MODE_WRITEBACK
			    : CACHE_MODE_WRITETHROUGH);

	if (data_offset != BDEV_DATA_START_DEFAULT) {
		sb.version = BCACHE_SB_VERSION_BDEV_WITH_OFFSET;
		sb.data_offset = data_offset;
	}

	sb.csum = csum_set(&sb, BCH_CSUM_CRC64);

	printf("UUID:			%s\n"
	       "Set UUID:		%s\n"
	       "version:		%u\n"
	       "block_size:		%u\n"
	       "data_offset:		%ju\n",
	       uuid_str, set_uuid_str,
	       (unsigned) sb.version,
	       sb.block_size,
	       data_offset);

	do_write_sb(fd, &sb);
}

int dev_open(const char *dev, bool wipe_bcache)
{
	struct cache_sb sb;
	blkid_probe pr;
	int fd;
	char err[256];

	if ((fd = open(dev, O_RDWR|O_EXCL)) == -1) {
		sprintf(err, "Can't open dev %s: %s\n", dev, strerror(errno));
		goto err;
	}

	if (pread(fd, &sb, sizeof(sb), SB_START) != sizeof(sb)) {
		sprintf(err, "Failed to read superblock");
		goto err;
	}

	if (!memcmp(&sb.magic, &BCACHE_MAGIC, 16) && !wipe_bcache) {
		sprintf(err, "Already a bcache device on %s, "
			"overwrite with --wipe-bcache\n", dev);
		goto err;
	}

	if (!(pr = blkid_new_probe())) {
		sprintf(err, "Failed to create a new probe");
		goto err;
	}
	if (blkid_probe_set_device(pr, fd, 0, 0)) {
		sprintf(err, "failed to set probe to device");
		goto err;
	}
	/* enable ptable probing; superblock probing is enabled by default */
	if (blkid_probe_enable_partitions(pr, true)) {
		sprintf(err, "Failed to enable partitions on probe");
		goto err;
	}
	if (!blkid_do_probe(pr)) {
		/* XXX wipefs doesn't know how to remove partition tables */
		sprintf(err, "Device %s already has a non-bcache superblock, "
			"remove it using wipefs and wipefs -a\n", dev);
		goto err;
	}

	return fd;

	err:
		fprintf(stderr, "dev_open failed with: %s", err);
		exit(EXIT_FAILURE);
}

static unsigned min_bucket_size(int num_bucket_sizes, unsigned *bucket_sizes)
{
	int i;
	unsigned min = bucket_sizes[0];

	for (i = 0; i < num_bucket_sizes; i++)
		min = bucket_sizes[i] < min ? bucket_sizes[i] : min;

	return min;
}

static unsigned node_size(unsigned bucket_size) {

	if (bucket_size <= 256)
		return bucket_size;
	else if (bucket_size <= 512)
		return bucket_size / 2;
	else
		return bucket_size / 4;
}

void write_cache_sbs(int *fds, struct cache_sb *sb,
			    unsigned block_size, unsigned *bucket_sizes,
				int num_bucket_sizes)
{
	char uuid_str[40], set_uuid_str[40];
	size_t i;
	unsigned min_size = min_bucket_size(num_bucket_sizes, bucket_sizes);

	sb->offset	= SB_SECTOR;
	sb->version	= BCACHE_SB_VERSION_CDEV_V3;
	sb->magic	= BCACHE_MAGIC;
	sb->block_size	= block_size;
	sb->keys	= bch_journal_buckets_offset(sb);

	/*
	 * don't have a userspace crc32c implementation handy, just always use
	 * crc64
	 */
	SET_CACHE_SB_CSUM_TYPE(sb, BCH_CSUM_CRC64);

	for (i = 0; i < sb->nr_in_set; i++) {
		struct cache_member *m = sb->members + i;

		if (num_bucket_sizes <= 1)
			sb->bucket_size = bucket_sizes[0];
		else
			sb->bucket_size	= bucket_sizes[i];
		SET_CACHE_BTREE_NODE_SIZE(sb, node_size(min_size));

		sb->uuid = m->uuid;
		sb->nbuckets		= getblocks(fds[i]) / sb->bucket_size;
		sb->nr_this_dev		= i;
		sb->first_bucket	= (23 / sb->bucket_size) + 1;

		if (sb->nbuckets < 1 << 7) {
			fprintf(stderr, "Not enough buckets: %llu, need %u\n",
				sb->nbuckets, 1 << 7);
			exit(EXIT_FAILURE);
		}

		sb->csum = csum_set(sb, CACHE_SB_CSUM_TYPE(sb));

		uuid_unparse(sb->uuid.b, uuid_str);
		uuid_unparse(sb->set_uuid.b, set_uuid_str);
		printf("UUID:			%s\n"
		       "Set UUID:		%s\n"
		       "version:		%u\n"
		       "nbuckets:		%llu\n"
		       "block_size:		%u\n"
		       "bucket_size:		%u\n"
		       "nr_in_set:		%u\n"
		       "nr_this_dev:		%u\n"
		       "first_bucket:		%u\n",
		       uuid_str, set_uuid_str,
		       (unsigned) sb->version,
		       sb->nbuckets,
		       sb->block_size,
		       sb->bucket_size,
		       sb->nr_in_set,
		       sb->nr_this_dev,
		       sb->first_bucket);

		do_write_sb(fds[i], sb);
	}
}

void next_cache_device(struct cache_sb *sb,
			      unsigned replication_set,
			      unsigned tier,
			      unsigned replacement_policy,
			      bool discard)
{
	struct cache_member *m = sb->members + sb->nr_in_set;

	SET_CACHE_REPLICATION_SET(m, replication_set);
	SET_CACHE_TIER(m, tier);
	SET_CACHE_REPLACEMENT(m, replacement_policy);
	SET_CACHE_DISCARD(m, discard);
	uuid_generate(m->uuid.b);

	sb->nr_in_set++;
}

unsigned get_blocksize(const char *path)
{
	struct stat statbuf;

	if (stat(path, &statbuf)) {
		fprintf(stderr, "Error statting %s: %s\n",
			path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (S_ISBLK(statbuf.st_mode)) {
		/* check IO limits:
		 * BLKALIGNOFF: alignment_offset
		 * BLKPBSZGET: physical_block_size
		 * BLKSSZGET: logical_block_size
		 * BLKIOMIN: minimum_io_size
		 * BLKIOOPT: optimal_io_size
		 *
		 * It may be tempting to use physical_block_size,
		 * or even minimum_io_size.
		 * But to be as transparent as possible,
		 * we want to use logical_block_size.
		 */
		unsigned int logical_block_size;
		int fd = open(path, O_RDONLY);

		if (fd < 0) {
			fprintf(stderr, "open(%s) failed: %m\n", path);
			exit(EXIT_FAILURE);
		}
		if (ioctl(fd, BLKSSZGET, &logical_block_size)) {
			fprintf(stderr, "ioctl(%s, BLKSSZGET) failed: %m\n", path);
			exit(EXIT_FAILURE);
		}
		close(fd);
		return logical_block_size / 512;

	}
	/* else: not a block device.
	 * Why would we even want to write a bcache super block there? */

	return statbuf.st_blksize / 512;
}

long strtoul_or_die(const char *p, size_t max, const char *msg)
{
	errno = 0;
	long v = strtol(p, NULL, 10);
	if (errno || v < 0 || v >= max) {
		fprintf(stderr, "Invalid %s %zi\n", msg, v);
		exit(EXIT_FAILURE);
	}

	return v;
}

static void print_encode(char *in)
{
    char *pos;
	for (pos = in; *pos; pos++)
		if (isalnum(*pos) || strchr(".-_", *pos))
			putchar(*pos);
		else
			printf("%%%x", *pos);
}

static void show_super_common(struct cache_sb *sb, bool force_csum)
{
	char uuid[40];
	char label[SB_LABEL_SIZE + 1];
	uint64_t expected_csum;

	printf("sb.magic\t\t");
	if (!memcmp(&sb->magic, &BCACHE_MAGIC, sizeof(sb->magic))) {
		printf("ok\n");
	} else {
		printf("bad magic\n");
		fprintf(stderr, "Invalid superblock (bad magic)\n");
		exit(2);
	}

	printf("sb.first_sector\t\t%ju", (uint64_t) sb->offset);
	if (sb->offset == SB_SECTOR) {
		printf(" [match]\n");
	} else {
		printf(" [expected %ds]\n", SB_SECTOR);
		fprintf(stderr, "Invalid superblock (bad sector)\n");
		exit(2);
	}

	printf("sb.csum\t\t\t%ju", (uint64_t) sb->csum);
	expected_csum = csum_set(sb,
				 sb->version < BCACHE_SB_VERSION_CDEV_V3
				 ? BCH_CSUM_CRC64
				 : CACHE_SB_CSUM_TYPE(sb));
	if (sb->csum == expected_csum) {
		printf(" [match]\n");
	} else {
		printf(" [expected %" PRIX64 "]\n", expected_csum);
		if (!force_csum) {
			fprintf(stderr, "Corrupt superblock (bad csum)\n");
			exit(2);
		}
	}

	printf("sb.version\t\t%ju", (uint64_t) sb->version);
	switch (sb->version) {
		// These are handled the same by the kernel
		case BCACHE_SB_VERSION_CDEV:
		case BCACHE_SB_VERSION_CDEV_WITH_UUID:
			printf(" [cache device]\n");
			break;

		// The second adds data offset support
		case BCACHE_SB_VERSION_BDEV:
		case BCACHE_SB_VERSION_BDEV_WITH_OFFSET:
			printf(" [backing device]\n");
			break;

		default:
			printf(" [unknown]\n");
			// exit code?
			exit(EXIT_SUCCESS);
	}

	putchar('\n');

	strncpy(label, (char *) sb->label, SB_LABEL_SIZE);
	label[SB_LABEL_SIZE] = '\0';
	printf("dev.label\t\t");
	if (*label)
		print_encode(label);
	else
		printf("(empty)");
	putchar('\n');

	uuid_unparse(sb->uuid.b, uuid);
	printf("dev.uuid\t\t%s\n", uuid);

	uuid_unparse(sb->set_uuid.b, uuid);
	printf("cset.uuid\t\t%s\n", uuid);
}

void show_super_backingdev(struct cache_sb *sb, bool force_csum)
{
	uint64_t first_sector;

	show_super_common(sb, force_csum);

	if (sb->version == BCACHE_SB_VERSION_BDEV) {
		first_sector = BDEV_DATA_START_DEFAULT;
	} else {
		if (sb->keys == 1 || sb->d[0]) {
			fprintf(stderr,
				"Possible experimental format detected, bailing\n");
			exit(3);
		}
		first_sector = sb->data_offset;
	}

	printf("dev.data.first_sector\t%ju\n"
	       "dev.data.cache_mode\t%s"
	       "dev.data.cache_state\t%s\n",
	       first_sector,
	       bdev_cache_mode[BDEV_CACHE_MODE(sb)],
	       bdev_state[BDEV_STATE(sb)]);
}

static void show_cache_member(struct cache_sb *sb, unsigned i)
{
	struct cache_member *m = ((struct cache_member *) sb->d) + i;

	printf("cache.state\t%s\n",		cache_state[CACHE_STATE(m)]);
	printf("cache.tier\t%llu\n",		CACHE_TIER(m));

	printf("cache.replication_set\t%llu\n",	CACHE_REPLICATION_SET(m));
	printf("cache.cur_meta_replicas\t%llu\n", REPLICATION_SET_CUR_META_REPLICAS(m));
	printf("cache.cur_data_replicas\t%llu\n", REPLICATION_SET_CUR_DATA_REPLICAS(m));

	printf("cache.has_metadata\t%llu\n",	CACHE_HAS_METADATA(m));
	printf("cache.has_data\t%llu\n",	CACHE_HAS_DATA(m));

	printf("cache.replacement\t%s\n",	replacement_policies[CACHE_REPLACEMENT(m)]);
	printf("cache.discard\t%llu\n",		CACHE_DISCARD(m));
}

void show_super_cache(struct cache_sb *sb, bool force_csum)
{
	show_super_common(sb, force_csum);

	printf("dev.sectors_per_block\t%u\n"
	       "dev.sectors_per_bucket\t%u\n",
	       sb->block_size,
	       sb->bucket_size);

	// total_sectors includes the superblock;
	printf("dev.cache.first_sector\t%u\n"
	       "dev.cache.cache_sectors\t%llu\n"
	       "dev.cache.total_sectors\t%llu\n"
	       "dev.cache.ordered\t%s\n"
	       "dev.cache.pos\t\t%u\n"
	       "dev.cache.setsize\t\t%u\n",
	       sb->bucket_size * sb->first_bucket,
	       sb->bucket_size * (sb->nbuckets - sb->first_bucket),
	       sb->bucket_size * sb->nbuckets,
	       CACHE_SYNC(sb) ? "yes" : "no",
	       sb->nr_this_dev,
	       sb->nr_in_set);

	show_cache_member(sb, sb->nr_this_dev);
}

struct cache_sb *query_dev(char *dev, bool force_csum)
{
	struct cache_sb sb_stack, *sb = &sb_stack;
	size_t bytes = sizeof(*sb);

	int fd = open(dev, O_RDONLY);
	if (fd < 0) {
		printf("Can't open dev %s: %s\n", dev, strerror(errno));
		exit(2);
	}

	if (pread(fd, sb, bytes, SB_START) != bytes) {
		fprintf(stderr, "Couldn't read\n");
		exit(2);
	}

	if (sb->keys) {
		bytes = sizeof(*sb) + sb->keys * sizeof(uint64_t);
		sb = malloc(bytes);

		if (pread(fd, sb, bytes, SB_START) != bytes) {
			fprintf(stderr, "Couldn't read\n");
			exit(2);
		}
	}

	return sb;
}

void print_dev_info(struct cache_sb *sb, bool force_csum)
{
	if (!SB_IS_BDEV(sb))
		show_super_cache(sb, force_csum);
	else
		show_super_backingdev(sb, force_csum);
}

int list_cachesets(char *cset_dir)
{
	struct dirent *ent;
	DIR *dir = opendir(cset_dir);
	if (!dir) {
		fprintf(stderr, "Failed to open dir %s\n", cset_dir);
		return 1;
	}

	while ((ent = readdir(dir)) != NULL) {
		struct stat statbuf;
		char entry[100];

		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
			continue;

		strcpy(entry, cset_dir);
		strcat(entry, "/");
		strcat(entry, ent->d_name);
		if(stat(entry, &statbuf) == -1) {
			fprintf(stderr, "Failed to stat %s\n", entry);
			return 1;
		}
		if (S_ISDIR(statbuf.st_mode)) {
			printf("%s\n", ent->d_name);
		}
	}

	closedir(dir);

	return 0;
}

char *parse_array_to_list(char *const *args)
{
	int i, len = 0;
	char *space = " ";
	for(i=0; args[i] != NULL; i++) {
		len+=strlen(args[i]) + 1;
	}

	char *arg_list = (char*)malloc(sizeof(char)*len);
	strcpy(arg_list, args[0]);
	strcat(arg_list, space);

	for(i=1; args[i] != NULL; i++) {
		strcat(arg_list, args[i]);
		strcat(arg_list, space);
	}

	return arg_list;
}

int register_bcache(char *devs)
{
	int ret, bcachefd;

	bcachefd = open("/dev/bcache", O_RDWR);
	if (bcachefd < 0) {
		perror("Can't open bcache device");
		exit(EXIT_FAILURE);
	}

	ret = ioctl(bcachefd, BCH_IOCTL_REGISTER, devs);
	if (ret < 0) {
		fprintf(stderr, "ioctl register error: %s", strerror(ret));
		exit(EXIT_FAILURE);
	}
	return 0;

}

int probe(char *dev, int udev)
{
	struct cache_sb sb;
	char uuid[40];
	blkid_probe pr;
	char *err = NULL;

	int fd = open(dev, O_RDONLY);
	if (fd == -1) {
		err = "Got file descriptor -1 trying to open dev";
		goto err;
	}

	if (!(pr = blkid_new_probe())) {
		err = "Failed trying to get a blkid for new probe";
		goto err;
	}

	if (blkid_probe_set_device(pr, fd, 0, 0)) {
		err = "Failed blkid probe set device";
		goto err;
	}

	/* probe partitions too */
	if (blkid_probe_enable_partitions(pr, true)) {
		err = "Enable probe partitions";
		goto err;
	}

	/* bail if anything was found
	 * probe-bcache isn't needed once blkid recognizes bcache */
	if (!blkid_do_probe(pr)) {
		err = "blkid recognizes bcache";
		goto err;
	}

	if (pread(fd, &sb, sizeof(sb), SB_START) != sizeof(sb)) {
		err = "Failed to read superblock";
		goto err;
	}

	if (memcmp(&sb.magic, &BCACHE_MAGIC, sizeof(sb.magic))) {
		err = "Bcache magic incorrect";
		goto err;
	}

	uuid_unparse(sb.uuid.b, uuid);

	if (udev)
		printf("ID_FS_UUID=%s\n"
		       "ID_FS_UUID_ENC=%s\n"
		       "ID_FS_TYPE=bcache\n",
		       uuid, uuid);
	else
		printf("%s: UUID=\"\" TYPE=\"bcache\"\n", uuid);

	return 0;

	err:
		fprintf(stderr, "Probe exit with error: %s", err);
		return -1;
}

void sb_state(struct cache_sb *sb, char *dev)
{
	struct cache_member *m = ((struct cache_member *) sb->d) +
		sb->nr_this_dev;

	printf("device %s\n", dev);
	printf("\tcache state\t%s\n",	cache_state[CACHE_STATE(m)]);
	printf("\tcache_tier\t%llu\n", CACHE_TIER(m));
	printf("\tseq#: \t%llu\n", sb->seq);

}

void read_stat_dir(DIR *dir, char *stats_dir, char *stat_name, bool print_val)
{
	struct stat statbuf;
	char entry[150];

	strcpy(entry, stats_dir);
	strcat(entry, "/");
	strcat(entry, stat_name);
	if(stat(entry, &statbuf) == -1) {
		fprintf(stderr, "Failed to stat %s\n", entry);
		return;
	}

	if (S_ISREG(statbuf.st_mode)) {
		char buf[100];
		FILE *fp = NULL;

		fp = fopen(entry, "r");
		if(!fp) {
			/* If we can't open the file, this is probably because
			 * of permissions, just move to the next file */
			return;
		}

		while(fgets(buf, 100, fp));

		if(print_val)
			printf("%s\t%s", stat_name, buf);
		else
			printf("%s\n", stat_name);
		fclose(fp);
	}
}
