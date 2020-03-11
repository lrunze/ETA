import logging
import os
import time

from etabackend.etalang.jit_linker import PARSE_TimeTagFileHeader_wrapper, np


class Clip():
    ETACReaderStructIDX = {
        # defined in PARSE_TimeTags.cpp#L55
        "fseekpoint": 0,
        "headeroffset": 1,

        "TTRes_pspr": 2,
        "DTRes_pspr": 3,
        "SYNCRate_pspr": 4,
        "BytesofRecords": 5,
        "RecordType": 6,

        "GlobalTimeShift": 7,
        "CHANNEL_OFFSET": 8,
        "MARKER_OFFSET": 9,

        "batch_actualread_length": 10,
        "next_RecID_in_batch": 11,
        "overflowcorrection": 12,
        "buffer_status": 13,
        "buffer": 14,
    }

    def __init__(self):
        self.fseekpoint = 0
        self.headeroffset = 0

        self.TTRes_pspr = 0
        self.DTRes_pspr = 0
        self.SYNCRate_pspr = 0
        self.BytesofRecords = 0
        self.RecordType = 0

        self.GlobalTimeShift = 0
        self.CHANNEL_OFFSET = 0
        self.MARKER_OFFSET = 0

        # UniBuf info
        self.batch_actualread_length = 0  # buffer length
        self.next_RecID_in_batch = 0  # reader head
        self.overflowcorrection = 0
        self.buffer_status = 0  # unused

        self.buffer = None

    def check_consumed(self):
        return self.next_RecID_in_batch * self.BytesofRecords >= self.batch_actualread_length

    def validate(self):
        self.GlobalTimeShift = int(self.GlobalTimeShift)
        self.TTRes_pspr = int(self.TTRes_pspr)
        self.DTRes_pspr = int(self.DTRes_pspr)
        self.SYNCRate_pspr = int(self.SYNCRate_pspr)
        if self.batch_actualread_length > len(self.buffer):
            raise ValueError(
                "batch_actualread_length is larger than the size of buffer")
        if self.batch_actualread_length == 0:
            return None
        return self

    def from_parser_output(self, parse_output):
        # print("======from_parser_output======")
        for name, v in self.ETACReaderStructIDX.items():
            if v < len(parse_output):
                value = int(parse_output[v])
                #print(value, "is", name)
                if name == "buffer":
                    value = None
                setattr(self, name, value)

    def to_reader_input(self):
        # print("======to_reader_input======")
        inv_map = {v: k for k, v in self.ETACReaderStructIDX.items()}
        ret = []
        for i in range(0,  len(inv_map)):
            name = inv_map[i]
            value = getattr(self, name)
            if isinstance(value, float) or isinstance(value, int):
                pass
            else:
                # print(type(value))
                value = -1
            #print(value, "is", name)
            ret.append(int(value))
        return ret

    def parse_header(self, filename):
        fileheader = bytearray(1024*20)  # 20KB header
        with open(filename, "rb") as f:
            f.readinto(fileheader)
        PARSER = np.array(self.to_reader_input(), np.int64)
        ret1 = PARSE_TimeTagFileHeader_wrapper(PARSER, fileheader)
        if ret1 != 0:
            raise ValueError(
                "Clip.parse_header: File {} is incorrect, err code {}.".format(filename, ret1))
        self.from_parser_output(PARSER)

    def get_pos(self):
        return self.fseekpoint+self.next_RecID_in_batch*self.BytesofRecords

class ETA_CUT():
    def __init__(self):
        self.logger = logging.getLogger(__name__)
        self.logfrontend = logging.getLogger("etabackend.frontend")

    # example generators
    def split_file(self, filename, cuts=1, reuse_clips=True, keep_indexes=None, **kwargs):
        # build a clip for header
        last_clip = self.clip_file(
            filename, read_events=1,  **kwargs)
        TTF_header_offset = last_clip.fseekpoint
        # reset the clip
        last_clip.fseekpoint = last_clip.headeroffset
        last_clip.batch_actualread_length = 0
        TTF_filesize = os.path.getsize(str(filename))

        NumRecords = (
            TTF_filesize - TTF_header_offset) // last_clip.BytesofRecords
        self.logfrontend.info("ETA.split_file: The file '{filename}' will be cut into {cuts} equal size Clips. ".format(filename=filename,
                                                                                                                        cuts=cuts))
        return self.clips(filename, read_events=((NumRecords+cuts-1) // cuts), modify_clip=last_clip, reuse_clips=reuse_clips, keep_indexes=keep_indexes, **kwargs)

    def clips(self, filename, modify_clip=None, read_events=1024*1024*10, reuse_clips=True, keep_indexes=None, **kwargs):
        last_clip = modify_clip
        currentclip = True
        counter = 0
        TTF_filesize = os.path.getsize(str(filename))

        while currentclip:
            if reuse_clips:
                currentclip = last_clip
            else:
                currentclip = Clip()
                # copy information from last clip
                currentclip.from_parser_output(last_clip.to_reader_input())
            # compute seeking info
            if keep_indexes:
                if type(keep_indexes) != list:
                    raise ValueError(
                        "ETA.clips: keep_indexes should be a list of indexes of sliding windows that you want to actually yield. ")
                if counter >= len(keep_indexes):
                    # keep_indexes reaching its end
                    break
                seek_event = keep_indexes[counter]*read_events
            else:
                seek_event = -1  # auto slide window
                # seek_event = counter * read_events # remove me
            currentclip = self.clip_file(
                filename, modify_clip=currentclip, read_events=read_events, seek_event=seek_event, **kwargs)
            if currentclip:
                self.logfrontend.info("Analysis progress: {:.2f}% ({:.2f}MB/{:.2f}MB)".format(
                    (currentclip.fseekpoint/TTF_filesize)*100.0, currentclip.fseekpoint/(1024.0*1024.0), TTF_filesize/(1024.0*1024.0)))
                yield currentclip
                counter += 1
                last_clip = currentclip
            else:
                # file reaching end
                break

    # low-level API

    def clip_file(self, filename, modify_clip=None, read_events=0, seek_event=-1, format=-1, wait_timeout=0):
        filename = str(filename)  # supporting pathlib
        if modify_clip == None:
            temp_clip = Clip()
            temp_clip.RecordType = format
            temp_clip.parse_header(filename)
        else:
            temp_clip = modify_clip

        # validate
        if not isinstance(temp_clip, Clip):
            raise ValueError(
                "ETA.clip_file: modify_clip must take a Clip object as the input.")

        # moving to the end of the last read or seek_event
        if seek_event < 0:
            temp_clip.fseekpoint += temp_clip.batch_actualread_length
        else:
            temp_clip.fseekpoint = temp_clip.headeroffset + \
                temp_clip.BytesofRecords * seek_event

        # use actual size of the file
        fileactualsize = os.path.getsize(filename)
        if read_events <= 0:
            read_events += (fileactualsize -
                            temp_clip.fseekpoint)//temp_clip.BytesofRecords
        # read at least one record each time
        if read_events <= 0:
            read_events = 1

        # compute the desired file size
        filedesiredsize = temp_clip.fseekpoint + \
            temp_clip.BytesofRecords * read_events

        # wait for data
        waited_for = 0.0
        while not fileactualsize >= filedesiredsize:
            waited_for += 0.01
            if waited_for > wait_timeout:
                self.logfrontend.info(
                    "ETA.clip_file: Timeout when waiting for data, overriding read_events to the current file boundry.")
                filedesiredsize = fileactualsize
                break
            self.logfrontend.info("ETA.clip_file: Waiting for file {} to grow from {} to {} bytes.".format(filename,
                                                                                                           fileactualsize,
                                                                                                           filedesiredsize))
            # hard-coded checking period is probably not good.
            time.sleep(0.01)

        # make buffer
        if temp_clip.fseekpoint > fileactualsize:
            self.logfrontend.info(
                "ETA.clip_file: Can not seek to {} in the file '{}' for the Clip, None is returned. ".format(temp_clip.fseekpoint, filename))
            return None
        if (temp_clip.buffer == None or len(temp_clip.buffer) != filedesiredsize-temp_clip.fseekpoint):
            # reuser buffer as much as possible
            temp_clip.buffer = bytearray(
                filedesiredsize-temp_clip.fseekpoint)
        # load data
        with open(filename, "rb") as f:
            f.seek(temp_clip.fseekpoint)
            temp_clip.batch_actualread_length = f.readinto(temp_clip.buffer)
        # fail when zero size
        if temp_clip.batch_actualread_length == 0:
            self.logfrontend.info(
                "ETA.clip_file: The file '{}' is not long enough for the Clip, None is returned. ".format(filename))
            return None
        else:
            self.logfrontend.info("ETA.clip_file: The file '{}' section [{},{}) is loaded into the Clip. ".format(
                filename, temp_clip.fseekpoint,  temp_clip.fseekpoint+temp_clip.batch_actualread_length))

        return temp_clip.validate()
