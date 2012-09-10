/*
 * =============================================================================
 *
 *       Filename:  client-t.c
 *
 *    Description:  client testcase.
 *
 *        Created:  09/09/2012 02:55:40 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */
#include <stdio.h>
#include <stdlib.h>

#include <lib/tstring.h>
#include <ossc/client.h>

#define _OSS_CLIENT_H
#include <modules/oss_client.h>
#undef _OSS_CLIENT_H

#include <curl/curl.h>

/* *
 * 初始化 oss_client_t，内部使用
 * */
oss_client_t *
_client_initialize(
		const char *access_id, size_t access_id_len,
		const char *access_key, size_t access_key_len,
		const char *endpoint, size_t endpoint_len)
{
	assert(access_id != NULL);
	assert(access_key != NULL);
	assert(endpoint != NULL);

	size_t byte_of_char = sizeof(char);
	oss_client_t *client = NULL;

	client = (oss_client_t *)malloc(sizeof(oss_client_t));

	client->access_id= (char *)malloc(byte_of_char * access_id_len+ 1);
	client->access_key = (char *)malloc(byte_of_char * access_key_len + 1);
	client->endpoint = (char *)malloc(byte_of_char * endpoint_len + 1);

	strncpy(client->access_id, access_id, access_id_len);
	strncpy(client->access_key, access_key, access_key_len);
	strncpy(client->endpoint, endpoint, endpoint_len);

	(client->access_id)[access_id_len] = '\0';
	(client->access_key)[access_key_len] = '\0';
	(client->endpoint)[endpoint_len] = '\0';

	return client;
}

/* *
 * 初始化 oss_client_t
 * */
oss_client_t *
client_initialize(const char *access_id,
		const char *access_key)
{
	assert(access_id != NULL);
	assert(access_key != NULL);

	size_t access_id_len = strlen(access_id);
	size_t access_key_len = strlen(access_key);
	size_t endpoint_len = strlen(DEFAULT_OSS_HOST);

	return _client_initialize(access_id, access_id_len,
			access_key, access_key_len,
			DEFAULT_OSS_HOST, endpoint_len);
}
size_t client_complete_multipart_upload_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
	size_t r = size * nmemb;
	strncpy(stream, ptr, r);
	return r;
}

/* *
 * 初始化complete_multipart_upload_request Object
 * */
oss_complete_multipart_upload_result_t*
client_complete_multipart_upload(oss_client_t *client,
		oss_complete_multipart_upload_request_t *request)
{

	assert(client != NULL);


	char resource[256]     = {0};
	//char request_line[256] = {0};
	char url[256]          = {0};
	char header_host[256]  = {0};
	char header_date[128]  = {0};
	char now[128]          = {0};
	char header_auth[512]  = {0};

	char headers[1024] = {0};
	char part[256] = {0};
	char response[1024] = {0};

	unsigned int sign_len = 0;
	int parts = 0;
	unsigned int i = 0;

	CURL *curl = NULL;
	CURLcode result;


	oss_map_t *default_headers = oss_map_new(16);

	sprintf(resource, "/%s/%s?uploadId=%s", request->get_bucket_name(request),
			request->get_key(request), request->get_upload_id(request));
	sprintf(url, "%s/%s/%s?uploadId=%s", client->endpoint, request->get_bucket_name(request),
			request->get_key(request), request->get_upload_id(request));
	sprintf(header_host,"Host: %s", client->endpoint);
	sprintf(now, "%s", oss_get_gmt_time());
	sprintf(header_date, "Date: %s", now);

	oss_map_put(default_headers, OSS_DATE, now);
	oss_map_put(default_headers, OSS_CONTENT_TYPE, "application/x-www-form-urlencoded");
	
	const char *sign = generate_authentication(client->access_key, OSS_HTTP_POST,
			default_headers, NULL, resource, &sign_len);

	sprintf(header_auth, "Authorization: OSS %s:%s", client->access_id, sign);

	oss_part_etag_t **part_etag = request->get_part_etags(request, &parts);
	tstring_t *tstr_part_etag = 
		tstring_new("<CompleteMultipartUpload>\n");
	for (; i < parts; i++) {
		sprintf(part, "\t<Part>\n\t\t<PartNumber>%d</PartNumber>\n\t\t<ETag>\"%s\"</ETag>\n\t</Part>\n",
				(*(part_etag + i))->get_part_number(*(part_etag + i)),
				(*(part_etag + i))->get_etag(*(part_etag + i)));
		tstring_append(tstr_part_etag, part);
	}
	tstring_append(tstr_part_etag, "</CompleteMultipartUpload>\n");
	printf("tstr_part_etag:\n%s\ntstr_part_etag_length: %d\n",
			tstring_data(tstr_part_etag), tstring_size(tstr_part_etag));
	


	curl = curl_easy_init();
	if (curl != NULL) {
		struct curl_slist *http_headers = NULL;
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURL_HTTP_VERSION_1_1, 1L);
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
		//curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
		//curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
		//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		//curl_easy_setopt(curl, CURLOPT_INFILESIZE, tstring_size(tstr_part_etag));
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, tstring_data(tstr_part_etag));
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, client_complete_multipart_upload_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);


		http_headers = curl_slist_append(http_headers, header_host);
		http_headers = curl_slist_append(http_headers, header_date);
		http_headers = curl_slist_append(http_headers, header_auth);

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_headers);
		curl_easy_perform(curl);

		curl_slist_free_all(http_headers);
		curl_easy_cleanup(curl);
	}
	printf("%s", response);

	return NULL;
}

const char *etags[] = {
	"AAE92ECB4974F96540B1D121869E5312",
	"76640000ED91DCB8433AC965AD61A717",
	"E6A2B713FB8F59918C634498138F50C7"
};

int main()
{
	const char *access_id = "ACSGmv8fkV1TDO9L";
	const char *access_key = "BedoWbsJe2";
	const char *bucket_name = "bucketname001";
	const char *key = "a_very_large_file.dat";
	const char *upload_id = "0004C955A174CDE5054ABAF82B1F99A9";

	oss_part_etag_t **part_etag = (oss_part_etag_t **)malloc(sizeof(oss_part_etag_t *) * 3);
	int i = 0;
	for (; i < 3; i++) {
		*(part_etag + i) = part_etag_initialize(i + 1, etags[i]);
	}

	oss_client_t *client = client_initialize(access_id, access_key);

	oss_complete_multipart_upload_request_t *request = 
		complete_multipart_upload_request_initialize(bucket_name, key, upload_id, part_etag, 3);

	client_complete_multipart_upload(client, request);
}
