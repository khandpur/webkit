/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Brent Fulgham <bfulgham@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Download.h"

#include "DataReference.h"
#include "DownloadSoupErrors.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceHandleInternal.h>
#include <gio/gio.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

#if PLATFORM(GTK)
#include <glib/gi18n-lib.h>
#endif

using namespace WebCore;

namespace WebKit {

class DownloadClient final : public ResourceHandleClient {
    WTF_MAKE_NONCOPYABLE(DownloadClient);
public:
    DownloadClient(Download& download)
        : m_download(download)
        , m_handleResponseLater(RunLoop::main(), this, &DownloadClient::handleResponse)
        , m_allowOverwrite(false)
    {
    }

    ~DownloadClient()
    {
    }

    void deleteFilesIfNeeded()
    {
        if (m_destinationFile)
            g_file_delete(m_destinationFile.get(), nullptr, nullptr);

        if (m_intermediateFile) {
            ASSERT(m_destinationFile);
            g_file_delete(m_intermediateFile.get(), nullptr, nullptr);
        }
    }

    void downloadFailed(const ResourceError& error)
    {
        deleteFilesIfNeeded();
        m_download.didFail(error, IPC::DataReference());
    }

    void didReceiveResponse(ResourceHandle*, ResourceResponse&& response) override
    {
        m_response = WTFMove(response);
        m_download.didReceiveResponse(m_response);

        if (m_response.httpStatusCode() >= 400) {
            downloadFailed(platformDownloadNetworkError(m_response.httpStatusCode(), m_response.url(), m_response.httpStatusText()));
            return;
        }

        String suggestedFilename = m_response.suggestedFilename();
        if (suggestedFilename.isEmpty()) {
            URL url = m_response.url();
            url.setQuery(String());
            url.removeFragmentIdentifier();
            suggestedFilename = decodeURLEscapeSequences(url.lastPathComponent());
        }

        String destinationURI = m_download.decideDestinationWithSuggestedFilename(suggestedFilename, m_allowOverwrite);
        if (destinationURI.isEmpty()) {
#if PLATFORM(GTK)
            GUniquePtr<char> buffer(g_strdup_printf(_("Cannot determine destination URI for download with suggested filename %s"), suggestedFilename.utf8().data()));
            String errorMessage = String::fromUTF8(buffer.get());
#else
            String errorMessage = makeString("Cannot determine destination URI for download with suggested filename ", suggestedFilename);
#endif
            downloadFailed(platformDownloadDestinationError(m_response, errorMessage));
            return;
        }

        m_destinationFile = adoptGRef(g_file_new_for_uri(destinationURI.utf8().data()));
        GRefPtr<GFileOutputStream> outputStream;
        GUniqueOutPtr<GError> error;
        if (m_allowOverwrite)
            outputStream = adoptGRef(g_file_replace(m_destinationFile.get(), nullptr, FALSE, G_FILE_CREATE_NONE, nullptr, &error.outPtr()));
        else
            outputStream = adoptGRef(g_file_create(m_destinationFile.get(), G_FILE_CREATE_NONE, nullptr, &error.outPtr()));
        if (!outputStream) {
            m_destinationFile.clear();
            downloadFailed(platformDownloadDestinationError(m_response, error->message));
            return;
        }

        String intermediateURI = destinationURI + ".wkdownload";
        m_intermediateFile = adoptGRef(g_file_new_for_uri(intermediateURI.utf8().data()));
        m_outputStream = adoptGRef(g_file_replace(m_intermediateFile.get(), 0, TRUE, G_FILE_CREATE_NONE, 0, &error.outPtr()));
        if (!m_outputStream) {
            downloadFailed(platformDownloadDestinationError(m_response, error->message));
            return;
        }

        m_download.didCreateDestination(destinationURI);
    }

    void didReceiveData(ResourceHandle*, const char* data, unsigned length, int /*encodedDataLength*/) override
    {
        if (m_handleResponseLater.isActive()) {
            m_handleResponseLater.stop();
            handleResponse();
        }

        gsize bytesWritten;
        GUniqueOutPtr<GError> error;
        g_output_stream_write_all(G_OUTPUT_STREAM(m_outputStream.get()), data, length, &bytesWritten, 0, &error.outPtr());
        if (error) {
            downloadFailed(platformDownloadDestinationError(m_response, error->message));
            return;
        }
        m_download.didReceiveData(bytesWritten);
    }

    void didFinishLoading(ResourceHandle*, double) override
    {
        m_outputStream = nullptr;

        ASSERT(m_destinationFile);
        ASSERT(m_intermediateFile);
        GUniqueOutPtr<GError> error;
        if (!g_file_move(m_intermediateFile.get(), m_destinationFile.get(), G_FILE_COPY_OVERWRITE, nullptr, nullptr, nullptr, &error.outPtr())) {
            downloadFailed(platformDownloadDestinationError(m_response, error->message));
            return;
        }

        GRefPtr<GFileInfo> info = adoptGRef(g_file_info_new());
        CString uri = m_response.url().string().utf8();
        g_file_info_set_attribute_string(info.get(), "metadata::download-uri", uri.data());
        g_file_info_set_attribute_string(info.get(), "xattr::xdg.origin.url", uri.data());
        g_file_set_attributes_async(m_destinationFile.get(), info.get(), G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT, nullptr, nullptr, nullptr);

        m_download.didFinish();
    }

    void didFail(ResourceHandle*, const ResourceError& error) override
    {
        downloadFailed(platformDownloadNetworkError(error.errorCode(), error.failingURL(), error.localizedDescription()));
    }

    void cancel(ResourceHandle* handle)
    {
        handle->cancel();
        deleteFilesIfNeeded();
        m_download.didCancel(IPC::DataReference());
    }

    void handleResponse()
    {
        didReceiveResponse(nullptr, WTFMove(m_delayedResponse));
    }

    void handleResponseLater(const ResourceResponse& response)
    {
        ASSERT(m_response.isNull());
        ASSERT(!m_handleResponseLater.isActive());

        m_delayedResponse = response;

        // Call didReceiveResponse in an idle to make sure the download is added
        // to the DownloadManager downloads map.
        m_handleResponseLater.startOneShot(0);
    }

    Download& m_download;
    GRefPtr<GFileOutputStream> m_outputStream;
    ResourceResponse m_response;
    GRefPtr<GFile> m_destinationFile;
    GRefPtr<GFile> m_intermediateFile;
    ResourceResponse m_delayedResponse;
    RunLoop::Timer<DownloadClient> m_handleResponseLater;
    bool m_allowOverwrite;
};

void Download::startNetworkLoad()
{
    ASSERT(!m_downloadClient);
    ASSERT(!m_resourceHandle);
    m_downloadClient = std::make_unique<DownloadClient>(*this);
    m_resourceHandle = ResourceHandle::create(0, m_request, m_downloadClient.get(), false, false);
    didStart();
}

void Download::startNetworkLoadWithHandle(ResourceHandle* resourceHandle, const ResourceResponse& response)
{
    ASSERT(!m_downloadClient);
    ASSERT(!m_resourceHandle);
    m_downloadClient = std::make_unique<DownloadClient>(*this);
    m_resourceHandle = resourceHandle->releaseForDownload(m_downloadClient.get());
    didStart();
    static_cast<DownloadClient*>(m_downloadClient.get())->handleResponseLater(response);
}

void Download::resume(const IPC::DataReference&, const String&, const SandboxExtension::Handle&)
{
    notImplemented();
}

void Download::cancelNetworkLoad()
{
    if (!m_resourceHandle)
        return;

    // Cancelling the download will delete it and platformInvalidate() will be called by the destructor.
    // So, we need to set m_resourceHandle to nullptr before actually cancelling the download to make sure
    // it won't be cancelled again by platformInvalidate. See https://bugs.webkit.org/show_bug.cgi?id=127650.
    RefPtr<ResourceHandle> resourceHandle = m_resourceHandle.release();
    static_cast<DownloadClient*>(m_downloadClient.get())->cancel(resourceHandle.get());
}

void Download::platformInvalidate()
{
}

void Download::platformDidFinish()
{
    m_resourceHandle = nullptr;
}

} // namespace WebKit
