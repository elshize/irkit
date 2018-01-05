"""Counts non-essential lists."""

import argparse
import json
import sys
import requests


def getparser():
    """Gets program's parser."""
    parser = argparse.ArgumentParser(
        prog='run', description='Client for Bloodhound server that counts'
                                'nonessential lists.')

    parser.add_argument('url', help='The URL of the server.')
    parser.add_argument('infile', nargs='?', type=argparse.FileType('r'),
                        default=sys.stdin, help='Input file (optional).')
    parser.add_argument('-k', nargs='?', type=int, default=30)
    parser.add_argument('--skip-header', action='store_true', default=False)
    return parser


def main():
    """Main function."""
    parser = getparser()
    args = parser.parse_args()
    if not args.skip_header:
        print('query',
              'terms',
              'threshold',
              'postings',
              'essential_terms_len',
              'essential_terms_ms',
              'nonessential_terms_len',
              'nonessential_terms_ms',
              'essential_postings_len',
              'essential_postings_ms',
              'essential_docs_len',
              'essential_docs_ms',
              'nonessential_updates_len',
              'nonessential_updates_ms',
              'allpost_len',
              'allpost_ms',
              sep=',')
        print('query',
              'max_score',
              'list_len',
              sep=',',
              file=sys.stderr)
    for idx, query in enumerate(args.infile):
        query = query[:-1]
        if '\t' in query:
            _, query = query.split('\t')
        request_data = {
            'type': 'ness',
            'k': args.k,
            'query': query
        }
        response = requests.post(args.url, json=request_data)
        resp_json = json.loads(response.text)
        print(idx,
              resp_json['stats']['terms'],
              resp_json['stats']['threshold'],
              resp_json['stats']['postings'],
              resp_json['stats']['essential_terms_len'],
              resp_json['stats']['essential_terms_ms'],
              resp_json['stats']['nonessential_terms_len'],
              resp_json['stats']['nonessential_terms_ms'],
              resp_json['stats']['essential_postings_len'],
              resp_json['stats']['essential_postings_ms'],
              resp_json['stats']['essential_docs_len'],
              resp_json['stats']['essential_docs_ms'],
              resp_json['stats']['nonessential_updates_len'],
              resp_json['stats']['nonessential_updates_ms'],
              resp_json['stats']['allpost_len'],
              resp_json['stats']['allpost_ms'],
              sep=',')
        for length, max_score in zip(resp_json['stats']['lengths'],
                                     resp_json['stats']['max_scores']):
            print(idx,
                  max_score,
                  length,
                  sep=',',
                  file=sys.stderr)


if __name__ == '__main__':
    main()
